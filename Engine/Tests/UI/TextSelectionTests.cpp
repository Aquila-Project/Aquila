#include <doctest.h>

#include "Aquila/UI/Text/TextSelection.h"

using namespace Aquila::UI::Text;

namespace {

float monospace(std::string_view text) {
	float width = 0.F;
	for (const char byte : text) {
		if ((static_cast<unsigned char>(byte) & 0xC0U) != 0x80U) {
			width += 8.F;
		}
	}
	return width;
}

const std::vector<std::string> k_lines = { "first line", "second", "third one" };

} // namespace

TEST_SUITE("TextSelection") {
	TEST_CASE("an empty selection yields no text") {
		CHECK(extract_text(k_lines, {}).empty());
		CHECK(extract_text({}, { { 0, 0 }, { 1, 3 } }).empty());
	}

	TEST_CASE("a selection inside one line is a substring") {
		CHECK(extract_text(k_lines, { { 0, 6 }, { 0, 10 } }) == "line");
	}

	TEST_CASE("a selection across lines joins them with newlines") {
		CHECK(extract_text(k_lines, { { 0, 6 }, { 2, 5 } }) == "line\nsecond\nthird");
	}

	TEST_CASE("selecting backwards gives the same text") {
		CHECK(extract_text(k_lines, { { 2, 5 }, { 0, 6 } }) == "line\nsecond\nthird");
	}

	TEST_CASE("out of range positions are clamped") {
		CHECK(extract_text(k_lines, { { 1, 0 }, { 9, 99 } }) == "second\nthird one");
		CHECK(extract_text(k_lines, { { 0, -5 }, { 0, 5 } }) == "first");
	}

	TEST_CASE("select all covers every line") {
		const TextSelection all = select_all(k_lines);
		CHECK(extract_text(k_lines, all) == "first line\nsecond\nthird one");
		CHECK(select_all({}).is_empty());
	}

	TEST_CASE("positions order by line then column") {
		CHECK(TextPosition{ 0, 9 } < TextPosition{ 1, 0 });
		CHECK(TextPosition{ 1, 2 } < TextPosition{ 1, 3 });
	}

	TEST_CASE("column_at snaps to the nearest character boundary") {
		CHECK(column_at("hello", -4.F, monospace) == 0);
		CHECK(column_at("hello", 0.F, monospace) == 0);
		CHECK(column_at("hello", 3.F, monospace) == 0);
		CHECK(column_at("hello", 5.F, monospace) == 1);
		CHECK(column_at("hello", 18.F, monospace) == 2);
		CHECK(column_at("hello", 500.F, monospace) == 5);
	}

	TEST_CASE("column_at moves by whole UTF-8 characters") {
		const std::string text = "a\xC3\xA9z";
		CHECK(column_at(text, 10.F, monospace) == 1);
		CHECK(column_at(text, 14.F, monospace) == 3);
		CHECK(column_at(text, 100.F, monospace) == 4);
	}
}
