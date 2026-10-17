#include <doctest.h>

#include "Aquila/Foundation/Log.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Style/StyleSheet.h"

using namespace Aquila::UI;
using Aquila::Foundation::Logger;
using Aquila::Foundation::LogLevel;

namespace {

struct Parsed {
	StyleSheet sheet;
	std::string log;
};

Parsed parse(std::string_view css) {
	Parsed parsed;
	std::ostringstream stream;
	const LogLevel previous = Logger::get_log_level();
	Logger::set_sink(&stream);
	Logger::set_log_level(LogLevel::Warning);
	StyleParser::LoadString(css, parsed.sheet);
	Logger::set_sink(nullptr);
	Logger::set_log_level(previous);
	parsed.log = stream.str();
	return parsed;
}

bool contains(const std::string &text, std::string_view needle) {
	return text.find(needle) != std::string::npos;
}

}

TEST_SUITE("StyleParser") {
	TEST_CASE("a state selector keeps its exact class name") {
		const Parsed parsed = parse(".tab:hover { width: 10px; }");
		CHECK(parsed.log.empty());
		REQUIRE(parsed.sheet.get_rules().size() == 1);
		CHECK(parsed.sheet.get_rules()[0].selector == "tab");
		CHECK(parsed.sheet.get_rules()[0].pseudo_class == "hover");
	}

	TEST_CASE("selectors with spaces are rejected instead of never matching") {
		for (const char *css : { ".tab :hover { width: 10px; }", ".a .b { width: 10px; }", ".a > .b { gap: 1px; }" }) {
			const Parsed parsed = parse(css);
			CHECK_MESSAGE(parsed.sheet.get_rules().empty(), css);
			CHECK_MESSAGE(contains(parsed.log, "unsupported selector"), css);
		}
	}

	TEST_CASE("unknown states are reported") {
		const Parsed parsed = parse(".a:hovr { gap: 1px; }");
		CHECK(parsed.sheet.get_rules().empty());
		CHECK(contains(parsed.log, "unknown state ':hovr'"));
	}

	TEST_CASE(":root only holds variables") {
		const Parsed parsed = parse(":root { --gap: 4px; }");
		CHECK(parsed.log.empty());
		CHECK(parsed.sheet.get_rules().empty());
		REQUIRE(parsed.sheet.get_variable("--gap") != nullptr);
		CHECK(*parsed.sheet.get_variable("--gap") == "4px");
	}

	TEST_CASE("a variable without a trailing semicolon ends at its block") {
		const Parsed parsed = parse(":root { --a: 4px } .b { gap: 2px; }");
		REQUIRE(parsed.sheet.get_variable("--a") != nullptr);
		CHECK(*parsed.sheet.get_variable("--a") == "4px");
		CHECK(parsed.sheet.get_rules().size() == 1);
	}

	TEST_CASE("selector names with double dashes are not variables") {
		const Parsed parsed = parse(".btn--primary:hover { --c: #ffffff; }");
		CHECK(parsed.sheet.get_variable("--primary") == nullptr);
		CHECK(parsed.sheet.get_variable("--c") != nullptr);
	}

	TEST_CASE("variables can reference variables in any order") {
		const Parsed parsed = parse(":root { --b: var(--a); --a: #ff0000; } .x { color: var(--b); }");
		CHECK(parsed.log.empty());
		REQUIRE(parsed.sheet.get_rules().size() == 1);
		const auto &color = parsed.sheet.get_rules()[0].properties.color;
		REQUIRE(color.has_value());
		CHECK(color->r == doctest::Approx(1.F));
		CHECK(color->g == doctest::Approx(0.F));
	}

	TEST_CASE("var() falls back and reports undefined variables once") {
		const Parsed fallback = parse(".x { color: var( --missing , #00ff00 ); }");
		CHECK(fallback.log.empty());
		REQUIRE(fallback.sheet.get_rules()[0].properties.color.has_value());
		CHECK(fallback.sheet.get_rules()[0].properties.color->g == doctest::Approx(1.F));

		const Parsed undefined = parse(".x { color: var(--missing); } .y { color: var(--missing); }");
		const auto first = undefined.log.find("undefined variable '--missing'");
		REQUIRE(first != std::string::npos);
		CHECK(undefined.log.find("undefined variable", first + 1) == std::string::npos);
	}

	TEST_CASE("a variable cycle terminates") {
		const Parsed parsed = parse(":root { --a: var(--b); --b: var(--a); } .x { color: var(--a); }");
		CHECK(contains(parsed.log, "cycle"));
	}

	TEST_CASE("nested at-rules are skipped without corrupting the block") {
		const Parsed parsed = parse("@media (max-width: 500px) { @media (min-width: 10px) { .a { gap: 1px; } } .b { "
									"gap: 2px; } } .c { gap: 3px; }");
		CHECK(contains(parsed.log, "nested at-rules"));
		REQUIRE(parsed.sheet.get_media_blocks().size() == 1);
		const auto &rules = parsed.sheet.get_media_blocks()[0].rules;
		REQUIRE(rules.size() == 1);
		CHECK(rules[0].selector == "b");
		REQUIRE(parsed.sheet.get_rules().size() == 1);
		CHECK(parsed.sheet.get_rules()[0].selector == "c");
	}

	TEST_CASE("broken declarations and at-rules are reported") {
		CHECK(contains(parse(".a { width 100%; }").log, "missing ':'"));
		CHECK(contains(parse(".a { gap: ; }").log, "needs 'property: value'"));
		CHECK(contains(parse("@font-face { gap: 1px; }").log, "unsupported at-rule"));
		CHECK(contains(parse("@media (max-widht: 5px) { .a { gap: 1px; } }").log, "unsupported condition"));
		CHECK(contains(parse(".a { gap: 1px; } /* open").log, "never closed"));
		CHECK(contains(parse(".a { gap: 1px; } stray").log, "at the end of the file"));
	}
}
