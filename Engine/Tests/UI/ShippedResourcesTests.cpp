#include <doctest.h>

#include "Aquila/Foundation/Log.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Style/StyleSheet.h"

using namespace Aquila::UI;
using Aquila::Foundation::Logger;
using Aquila::Foundation::LogLevel;

namespace {

const std::filesystem::path k_ui_dir = std::filesystem::path(AQUILA_SOURCE_DIR) / "Resources/Engine/UI";

std::vector<std::filesystem::path> files_with(std::string_view extension) {
	std::vector<std::filesystem::path> files;
	for (const auto &entry : std::filesystem::directory_iterator(k_ui_dir)) {
		if (entry.path().extension() == extension) {
			files.push_back(entry.path());
		}
	}
	std::ranges::sort(files, [](const auto &a, const auto &b) {
		return (a.stem() == "tokens") != (b.stem() == "tokens") ? a.stem() == "tokens" : a < b;
	});
	return files;
}

std::string read(const std::filesystem::path &path) {
	std::ifstream file(path);
	std::stringstream text;
	text << file.rdbuf();
	return text.str();
}

struct LogCapture {
	std::ostringstream stream;
	LogLevel previous = Logger::get_log_level();

	LogCapture() {
		Logger::set_sink(&stream);
		Logger::set_log_level(LogLevel::Warning);
	}

	~LogCapture() {
		Logger::set_sink(nullptr);
		Logger::set_log_level(previous);
	}
};

}

TEST_SUITE("Shipped UI resources") {
	TEST_CASE("every stylesheet parses without diagnostics") {
		const auto files = files_with(".aqstyle");
		REQUIRE_FALSE(files.empty());

		StyleSheet sheet;
		LogCapture capture;
		for (const auto &path : files) {
			CHECK_MESSAGE(StyleParser::LoadString(read(path), sheet, false), path.string());
		}
		CHECK_MESSAGE(capture.stream.str().empty(), capture.stream.str());
		CHECK(sheet.get_rule_count() > 0);
	}

	TEST_CASE("every layout parses without markup diagnostics") {
		const auto files = files_with(".aqlayout");
		REQUIRE_FALSE(files.empty());

		for (const auto &path : files) {
			LogCapture capture;
			Core::LayoutLoader loader;
			const auto root = loader.LoadString(read(path));
			CHECK_MESSAGE(root != nullptr, path.string());
			const std::string log = capture.stream.str();
			CHECK_MESSAGE(log.find("<string>:") == std::string::npos, (path.string() + "\n" + log));
		}
	}
}
