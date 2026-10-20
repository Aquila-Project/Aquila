#ifndef AQUILA_EDITOR_CORE_PROJECT_SETTINGS_H
#define AQUILA_EDITOR_CORE_PROJECT_SETTINGS_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Rendering/RenderSettings.h"
#include "json.hpp"

namespace Editor {

/// What a project ships with, saved under "settings" in project.aqproject. The Build window starts from it and the
/// Game view renders with it. Paths are relative to the project folder ("Scenes/Level.aqscene",
/// "Content/icon.png"); `build_output` is a folder on disk.
struct ProjectSettings {
	/// Empty = the first scene of the project.
	std::string startup_scene;
	/// A PNG, empty = the Aquila logo.
	std::string icon;
	/// Empty = the project name.
	std::string window_title;
	Uint32 window_width = 1280;
	Uint32 window_height = 720;
	bool start_maximized = false;
	Aquila::Rendering::ToneMapper tone_mapper = Aquila::Rendering::ToneMapper::AgX;
	bool shadows = true;
	/// Empty = Builds next to the editor.
	std::string build_output;

	bool operator==(const ProjectSettings &) const = default;
};

[[nodiscard]] nlohmann::ordered_json project_settings_to_json(const ProjectSettings &settings);
/// Missing or wrong keys keep their defaults.
[[nodiscard]] ProjectSettings project_settings_from_json(const nlohmann::ordered_json &json);
/// RenderSettings::game() with the project's tone mapper and shadows, what the Game view and built games draw with.
[[nodiscard]] Aquila::Rendering::RenderSettings game_render_settings(const ProjectSettings &settings);

}

#endif
