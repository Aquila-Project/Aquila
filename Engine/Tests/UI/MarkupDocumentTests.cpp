#include <doctest.h>

#include "Aquila/UI/Markup/MarkupParser.h"

using namespace Aquila::UI::Markup;

TEST_SUITE("Markup document") {
	TEST_CASE("a document keeps elements, attributes and text in order") {
		const MarkupDocument doc = parse_markup(R"(<View id="a"><Label>Hi</Label><View/></View>)", "<string>");
		REQUIRE(doc.root.has_value());
		CHECK(doc.root->tag == "View");
		REQUIRE(doc.root->find("id") != nullptr);
		CHECK(doc.root->find("id")->value == "a");
		REQUIRE(doc.root->children.size() == 2);
		CHECK(doc.root->children[0].tag == "Label");
		REQUIRE(doc.root->children[0].children.size() == 1);
		CHECK(doc.root->children[0].children[0].is_text());
		CHECK(doc.root->children[0].children[0].text == "Hi");
		CHECK(doc.root->children[1].self_closing);
	}

	TEST_CASE("locations are 1-based lines and columns") {
		const MarkupDocument doc = parse_markup("\n  <View/>", "x.aqlayout");
		REQUIRE(doc.root.has_value());
		CHECK(doc.root->where.line == 2);
		CHECK(doc.root->where.column == 3);
		CHECK(doc.location(doc.root->where) == "x.aqlayout:2:3");
	}

	TEST_CASE("a duplicate attribute keeps both and the last one is found") {
		const MarkupDocument doc = parse_markup(R"(<View id="a" id="b"/>)", "<string>");
		REQUIRE(doc.root.has_value());
		CHECK(doc.root->attributes.size() == 2);
		CHECK(doc.root->find("id")->value == "b");
	}
}
