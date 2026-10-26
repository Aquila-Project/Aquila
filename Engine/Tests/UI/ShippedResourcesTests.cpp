#include <doctest.h>

#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Style/StyleSheet.h"
#include "VfsTestSupport.h"

using namespace Aquila::UI;

namespace {

const std::filesystem::path K_UI_DIR = std::filesystem::path(AQUILA_SOURCE_DIR) / "Resources/Engine/UI";

std::vector<std::filesystem::path> files_with(std::string_view extension) {
	std::vector<std::filesystem::path> files;
	for (const auto &entry : std::filesystem::recursive_directory_iterator(K_UI_DIR)) {
		if (entry.path().extension() == extension) {
			files.push_back(entry.path());
		}
	}
	std::ranges::sort(files, [](const auto &a, const auto &b) {
		return (a.stem() == "tokens") != (b.stem() == "tokens") ? a.stem() == "tokens" : a < b;
	});
	return files;
}

std::string vfs_path(const std::filesystem::path &file) {
	return "/resources/Engine/UI/" + std::filesystem::relative(file, K_UI_DIR).generic_string();
}
}

TEST_SUITE("Shipped UI resources") {
	TEST_CASE("every stylesheet parses without diagnostics") {
		test_vfs();
		const auto files = files_with(".aqstyle");
		REQUIRE_FALSE(files.empty());

		std::vector<std::string> paths;
		for (const auto &file : files) {
			paths.push_back(vfs_path(file));
		}
		StyleSheet sheet;
		LogCapture capture;
		CHECK(StyleParser::load_files(paths, sheet));
		CHECK_MESSAGE(capture.stream.str().empty(), capture.stream.str());
		CHECK(sheet.get_rule_count() > 0);
	}

	TEST_CASE("every layout parses without markup diagnostics") {
		test_vfs();
		const auto files = files_with(".aqlayout");
		REQUIRE_FALSE(files.empty());

		for (const auto &file : files) {
			const std::string path = vfs_path(file);
			LogCapture capture;
			Core::LayoutLoader loader;
			const auto root = loader.load_file(path);
			if (file.parent_path().filename() != "Components") {
				CHECK_MESSAGE(root != nullptr, path);
			}
			const std::string log = capture.stream.str();
			CHECK_MESSAGE(log.find(path + ":") == std::string::npos, (path + "\n" + log));
		}
	}

	TEST_CASE("every window has its layout and its style in Windows") {
		CHECK(std::filesystem::exists(K_UI_DIR / "Windows/game.aqlayout"));
		for (const auto &file : files_with(".aqlayout")) {
			if (file.parent_path().filename() == "Windows") {
				CHECK_MESSAGE(std::filesystem::exists(std::filesystem::path(file).replace_extension(".aqstyle")),
							  file.string());
			}
		}
	}
}
