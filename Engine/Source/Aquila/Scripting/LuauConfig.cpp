#include "Aquila/Scripting/LuauConfig.h"

#include "Aquila/Foundation/Macros.h"
#include "json.hpp"

namespace Aquila::Scripting {

namespace {

using Json = nlohmann::ordered_json;

Option<Json> read_json(const std::filesystem::path &path) {
	std::error_code error;
	if (!std::filesystem::exists(path, error)) {
		return Json::object();
	}
	std::stringstream text;
	text << std::ifstream(path).rdbuf();
	const std::string content = text.str();
	if (content.find_first_not_of(" \t\r\n") == std::string::npos) {
		return Json::object();
	}
	Json parsed = Json::parse(content, nullptr, false, false);
	if (parsed.is_discarded() || !parsed.is_object()) {
		const bool commented = !Json::parse(content, nullptr, false, true).is_discarded();
		AQUILA_LOG_WARNING("Luau config: {} {}, so it's left alone", path.string(),
						   commented ? "has comments" : "isn't a JSON object");
		return std::nullopt;
	}
	return parsed;
}

bool write_json(const std::filesystem::path &path, const Json &before, const Json &after) {
	if (before == after) {
		return true;
	}
	std::error_code error;
	std::filesystem::create_directories(path.parent_path(), error);
	std::ofstream out(path, std::ios::trunc);
	out << after.dump(1, '\t') << '\n';
	if (!out) {
		AQUILA_LOG_WARNING("Luau config: couldn't write {}", path.string());
		return false;
	}
	return true;
}

}

std::map<std::string, std::string> engine_luau_aliases(const std::string &api_dir, const std::string &types_dir) {
	std::map<std::string, std::string> aliases{ { "aquila", api_dir } };
	std::error_code error;
	for (const auto &entry : std::filesystem::directory_iterator(types_dir, error)) {
		const std::string name = entry.path().filename().string();
		if (entry.is_regular_file() && name.ends_with(".luau") && !name.ends_with(".d.luau")) {
			const std::string module = entry.path().stem().string();
			aliases[module] = (std::filesystem::path(types_dir) / module).generic_string();
		}
	}
	return aliases;
}

bool update_luaurc(const std::string &directory, const std::map<std::string, std::string> &aliases) {
	const std::filesystem::path path = std::filesystem::path(directory) / ".luaurc";
	const Option<Json> before = read_json(path);
	if (!before) {
		return false;
	}
	Json after = *before;
	if (!after.contains("languageMode")) {
		after["languageMode"] = "strict";
	}
	if (!after.contains("aliases") || !after["aliases"].is_object()) {
		after["aliases"] = Json::object();
	}
	for (const auto &[name, target] : aliases) {
		after["aliases"][name] = target;
	}
	return write_json(path, *before, after);
}

bool update_vscode_settings(const std::string &directory, const std::string &definitions_file) {
	const std::filesystem::path path = std::filesystem::path(directory) / ".vscode" / "settings.json";
	const Option<Json> before = read_json(path);
	if (!before) {
		return false;
	}
	Json after = *before;
	constexpr const char *K_DEFINITIONS = "luau-lsp.types.definitionFiles";
	if (!after.contains(K_DEFINITIONS) || !after[K_DEFINITIONS].is_object()) {
		after[K_DEFINITIONS] = Json::object();
	}
	after[K_DEFINITIONS]["@aquila"] = definitions_file;
	return write_json(path, *before, after);
}

}
