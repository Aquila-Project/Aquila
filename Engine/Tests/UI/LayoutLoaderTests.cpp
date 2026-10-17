#include <doctest.h>

#include "Aquila/Foundation/Log.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/Label.h"

using namespace Aquila::UI::Core;
using Aquila::Foundation::Logger;
using Aquila::Foundation::LogLevel;

namespace {

struct Loaded {
	Unique<View> root;
	std::string log;
};

Loaded load(std::string_view xml) {
	std::ostringstream stream;
	const LogLevel previous = Logger::get_log_level();
	Logger::set_sink(&stream);
	Logger::set_log_level(LogLevel::Warning);
	LayoutLoader loader;
	Unique<View> root = loader.LoadString(xml);
	Logger::set_sink(nullptr);
	Logger::set_log_level(previous);
	return { std::move(root), stream.str() };
}

std::vector<std::string> child_ids(const View &view) {
	std::vector<std::string> ids;
	for (const auto &child : view.get_children()) {
		ids.push_back(child->get_id());
	}
	return ids;
}

}

TEST_SUITE("LayoutLoader") {
	TEST_CASE("a valid layout loads without diagnostics") {
		const Loaded loaded = load(R"(<?xml version="1.0"?>
<!-- header -->
<View id="root" class="a b">
	<View id="first" width='100px' height=20px/>
	<Label id="label">Hello there</Label>
</View>
<!-- trailing comment -->
)");
		REQUIRE(loaded.root != nullptr);
		CHECK_MESSAGE(loaded.log.empty(), loaded.log);
		CHECK(loaded.root->get_id() == "root");
		CHECK(loaded.root->get_classes() == std::vector<std::string>{ "a", "b" });
		REQUIRE(child_ids(*loaded.root) == std::vector<std::string>{ "first", "label" });
		const auto *label = dynamic_cast<const Label *>(loaded.root->get_children()[1].get());
		REQUIRE(label != nullptr);
		CHECK(label->get_text() == "Hello there");
	}

	TEST_CASE("unsupported <! markup is skipped instead of hanging") {
		const Loaded top = load("<!DOCTYPE layout>\n<View id=\"root\"/>");
		REQUIRE(top.root != nullptr);
		CHECK(top.log.find("only <!-- --> comments") != std::string::npos);

		const Loaded inside = load(R"(<View id="root"><![CDATA[x]]><View id="kid"/></View>)");
		REQUIRE(inside.root != nullptr);
		CHECK(child_ids(*inside.root) == std::vector<std::string>{ "kid" });

		const Loaded truncated = load("<View id=\"root\"/><!-");
		CHECK(truncated.root != nullptr);
	}

	TEST_CASE("an unterminated comment is reported") {
		const Loaded loaded = load("<View id=\"root\"><!-- never closed");
		REQUIRE(loaded.root != nullptr);
		CHECK(loaded.log.find("never closed with '-->'") != std::string::npos);
	}

	TEST_CASE("a mismatched closing tag is reported and closes the open element") {
		const Loaded loaded = load(R"(<View id="root"><View id="a"></Viwe><View id="b"/></View>)");
		REQUIRE(loaded.root != nullptr);
		CHECK(loaded.log.find("</Viwe> closes <View>") != std::string::npos);
		CHECK(child_ids(*loaded.root) == std::vector<std::string>{ "a", "b" });
	}

	TEST_CASE("an unclosed element is reported") {
		const Loaded loaded = load(R"(<View id="root"><View id="a">)");
		REQUIRE(loaded.root != nullptr);
		CHECK(loaded.log.find("<View> is never closed") != std::string::npos);
		CHECK(loaded.log.find(":1:17") != std::string::npos);
	}

	TEST_CASE("an unterminated quote is reported") {
		const Loaded loaded = load(R"(<View id="root><View/></View>)");
		CHECK(loaded.log.find("quote is never closed") != std::string::npos);
	}

	TEST_CASE("an unquoted value with a slash is reported and the tag still closes") {
		const Loaded loaded = load(R"(<View id="root"><View id=left/right/><View id="next"/></View>)");
		REQUIRE(loaded.root != nullptr);
		CHECK(loaded.log.find("stops at '/'") != std::string::npos);
		CHECK(child_ids(*loaded.root) == std::vector<std::string>{ "left", "next" });
	}

	TEST_CASE("an unknown widget skips its whole subtree") {
		const Loaded loaded = load(R"(<View id="root"><Nope><View id="inner"/></Nope><View id="after"/></View>)");
		REQUIRE(loaded.root != nullptr);
		CHECK(loaded.log.find("unknown widget type 'Nope'") != std::string::npos);
		CHECK(child_ids(*loaded.root) == std::vector<std::string>{ "after" });
	}

	TEST_CASE("junk inside a tag is reported") {
		const Loaded loaded = load(R"(<View id="root"><View "oops" id="x"/><View id="y"/></View>)");
		REQUIRE(loaded.root != nullptr);
		CHECK(loaded.log.find("expected an attribute name") != std::string::npos);
		CHECK(loaded.root->get_children().size() == 2);
	}

	TEST_CASE("<Include> without src is reported") {
		const Loaded loaded = load(R"(<View id="root"><Include/></View>)");
		REQUIRE(loaded.root != nullptr);
		CHECK(loaded.log.find("missing a 'src'") != std::string::npos);
		CHECK(loaded.root->get_children().empty());
	}

	TEST_CASE("a duplicate attribute is reported and the last one wins") {
		const Loaded loaded = load(R"(<View id="a" id="b"/>)");
		REQUIRE(loaded.root != nullptr);
		CHECK(loaded.log.find("given twice") != std::string::npos);
		CHECK(loaded.root->get_id() == "b");
	}

	TEST_CASE("content after the root and missing roots are reported") {
		CHECK(load("<View/>\n<View/>").log.find("content after the root") != std::string::npos);
		const Loaded empty = load("<!-- nothing -->");
		CHECK(empty.root == nullptr);
		CHECK(empty.log.find("no root element") != std::string::npos);
		CHECK(load("</View><View/>").log.find("before the root element") != std::string::npos);
	}

	TEST_CASE("an inline style declaration without a value is reported") {
		const Loaded loaded = load(R"(<View style="width 100%"/>)");
		REQUIRE(loaded.root != nullptr);
		CHECK(loaded.log.find("needs 'property: value'") != std::string::npos);
	}
}
