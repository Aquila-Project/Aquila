#include <doctest.h>

#include "Aquila/Foundation/Text/StringUtils.h"

using namespace Aquila::Foundation;

TEST_SUITE("StringUtils") {
	TEST_CASE("split_arguments splits on spaces and keeps quoted parts together") {
		CHECK(split_arguments("") == std::vector<std::string>{});
		CHECK(split_arguments("  {file}  ") == std::vector<std::string>{ "{file}" });
		CHECK(split_arguments("--goto {file}") == std::vector<std::string>{ "--goto", "{file}" });
		CHECK(split_arguments("-n \"{project}\" '{file}'") == std::vector<std::string>{ "-n", "{project}", "{file}" });
		CHECK(split_arguments("\"two words\" x") == std::vector<std::string>{ "two words", "x" });
		CHECK(split_arguments("a\"b c\"d") == std::vector<std::string>{ "ab cd" });
		CHECK(split_arguments("\"\"") == std::vector<std::string>{ "" });
		CHECK(split_arguments("\"open") == std::vector<std::string>{ "open" });
	}

	TEST_CASE("replace_all replaces every match and leaves what it puts in alone") {
		std::string text = "{file}:{file}";
		replace_all(text, "{file}", "/a b/{file}.luau");
		CHECK(text == "/a b/{file}.luau:/a b/{file}.luau");
		std::string unchanged = "nothing here";
		replace_all(unchanged, "{file}", "x");
		CHECK(unchanged == "nothing here");
		std::string empty_pattern = "abc";
		replace_all(empty_pattern, "", "x");
		CHECK(empty_pattern == "abc");
	}
}
