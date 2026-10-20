#include "Aquila/Player/BuildManifest.h"

#include "Aquila/Foundation/Macros.h"
#include "json.hpp"

namespace Aquila::Player {

std::string write_manifest(const BuildManifest &build_manifest) {
	if (!build_manifest.is_valid()) {
		AQUILA_LOG_ERROR("Build manifest contains errors");
		return {};
	}

	nlohmann::ordered_json manifest;
	manifest["version"] = build_manifest.version;
	manifest["name"] = build_manifest.name;
	manifest["startup_scene"] = build_manifest.startup_scene;
	manifest["window"] = { { "width", build_manifest.window_width },
						   { "height", build_manifest.window_height },
						   { "title", build_manifest.window_title },
						   { "maximized", build_manifest.maximized } };
	manifest["rendering"] = { { "tone_mapper", std::string(Rendering::tone_mapper_name(build_manifest.tone_mapper)) },
							  { "shadows", build_manifest.shadows } };

	return manifest.dump(4);
}

Option<BuildManifest> read_manifest(std::string_view text) {
	BuildManifest build_manifest{};

	nlohmann::ordered_json manifest_json = nlohmann::ordered_json::parse(text, nullptr, false);
	if (manifest_json.is_discarded() || !manifest_json.is_object()) {
		return std::nullopt;
	}

	if (!manifest_json.contains("version") || !manifest_json.contains("name") ||
		!manifest_json.contains("startup_scene")) {
		return std::nullopt;
	}

	try {
		if (manifest_json["version"] > build_manifest.version) {
			return std::nullopt;
		}

		build_manifest.version = manifest_json["version"];
		build_manifest.name = manifest_json["name"];
		build_manifest.startup_scene = manifest_json["startup_scene"];

		if (manifest_json.contains("window")) {
			if (manifest_json["window"].contains("width")) {
				build_manifest.window_width = manifest_json["window"]["width"];
			}
			if (manifest_json["window"].contains("height")) {
				build_manifest.window_height = manifest_json["window"]["height"];
			}
			build_manifest.window_title = manifest_json["window"].value("title", build_manifest.window_title);
			build_manifest.maximized = manifest_json["window"].value("maximized", build_manifest.maximized);
		}
		if (manifest_json.contains("rendering")) {
			const auto &rendering = manifest_json["rendering"];
			build_manifest.tone_mapper =
				Rendering::tone_mapper_from_name(rendering.value("tone_mapper", std::string("agx")));
			build_manifest.shadows = rendering.value("shadows", build_manifest.shadows);
		}
	} catch (const nlohmann::json::exception &) {
		return std::nullopt;
	}
	return build_manifest;
}

Option<BuildManifest> load_manifest(const std::string &file) {
	std::ifstream stream(file, std::ios::binary);
	if (!stream.is_open()) {
		return std::nullopt;
	}
	const std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	return read_manifest(content);
}

} // namespace Aquila::Player
