#include <doctest.h>

#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Style/StyleParserHelper.h"
#include "Aquila/UI/Style/StyleSheet.h"

using Aquila::UI::ParserHelper::parse_color;

TEST_CASE("parse_color accepts the transparent keyword") {
	const auto color = parse_color("transparent");
	REQUIRE(color.has_value());
	CHECK(color->a == doctest::Approx(0.F));
}

TEST_CASE("parse_color keeps hex and rgba parsing") {
	const auto hex = parse_color("#4a9e5f");
	REQUIRE(hex.has_value());
	CHECK(hex->a == doctest::Approx(1.F));

	const auto rgba = parse_color("rgba(255, 255, 255, 0.08)");
	REQUIRE(rgba.has_value());
	CHECK(rgba->a == doctest::Approx(0.08F));
}

TEST_CASE("a lone class rule gives its color") {
	Aquila::UI::StyleSheet sheet;
	REQUIRE(Aquila::UI::StyleParser::LoadString(".a { color: rgb(1, 0, 0); }", sheet));
	CHECK(sheet.find_class_color("a") == Vec4(1.F, 0.F, 0.F, 1.F));
}

TEST_CASE("the last class rule with a color wins") {
	Aquila::UI::StyleSheet sheet;
	REQUIRE(Aquila::UI::StyleParser::LoadString(
		".a { color: rgb(1, 0, 0); } .b { color: rgb(0, 1, 0); } .a { color: rgb(0, 0, 1); } .a { width: 1px; }",
		sheet));
	CHECK(sheet.find_class_color("a") == Vec4(0.F, 0.F, 1.F, 1.F));
}

TEST_CASE("pseudo-class rules and missing classes give no color") {
	Aquila::UI::StyleSheet sheet;
	REQUIRE(Aquila::UI::StyleParser::LoadString(".a:hover { color: rgb(1, 0, 0); } .b { width: 1px; }", sheet));
	CHECK_FALSE(sheet.find_class_color("a").has_value());
	CHECK_FALSE(sheet.find_class_color("b").has_value());
	CHECK_FALSE(sheet.find_class_color("nope").has_value());
}
