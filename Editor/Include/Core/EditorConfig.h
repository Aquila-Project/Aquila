#ifndef EDITOR_CONFIG_H
#define EDITOR_CONFIG_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/UI/Core/FontManager.h"

namespace Editor::Config {

constexpr const char *EDITOR_NAME = "Aquila Editor";
constexpr const char *EDITOR_VERSION = AQUILA_VERSION_STRING;
constexpr uint32_t VERSION_MAJOR = AQUILA_VERSION_MAJOR;
constexpr uint32_t VERSION_MINOR = AQUILA_VERSION_MINOR;
constexpr uint32_t VERSION_PATCH = AQUILA_VERSION_PATCH;
constexpr const char *EDITOR_BUILD_DATE = __DATE__;
constexpr const char *EDITOR_TITLE = "Aquila Editor " AQUILA_VERSION_STRING " | " __DATE__;

struct WindowSettings {
	Uint32 width = 1920;
	Uint32 height = 1080;
	bool maximized = false;
	bool vsync = true;
};

using FontSettings = Aquila::UI::Core::FontSettings;

struct UISettings {
	std::string resources_path = "/resources";
	std::string layout_path = "/resources/Engine/UI/editor.aqlayout";
	std::vector<std::string> style_paths = {
		"/resources/Engine/UI/tokens.aqstyle",		  "/resources/Engine/UI/widgets.aqstyle",
		"/resources/Engine/UI/dock.aqstyle",		  "/resources/Engine/UI/launcher.aqstyle",
		"/resources/Engine/UI/editor_chrome.aqstyle", "/resources/Engine/UI/devtools.aqstyle",
	};
};

enum class Theme { Aquila, Aquila2, Dark, Light, Custom };

/// Saved as JSON at preference_file_path.
struct EditorPreferences {
	WindowSettings window;
	FontSettings fonts;
	UISettings ui;
	Theme current_theme = Theme::Aquila2;

	F32 ui_scale = 1.0F;

	bool show_grid = true;
	bool show_gizmos = true;
	bool auto_save = false;
	Uint32 auto_save_interval = 300; // seconds

	F32 camera_move_speed = 5.0F;
	F32 camera_rotate_speed = 0.5F;
	bool invert_mouse_y = false;

	F32 thumbnail_size = 64.0F;
	bool show_file_extensions = true;

	std::string preference_file_path = "/app/editor_preferences.json";

	void load_from_file();
	void save_to_file() const;
	void reset_to_defaults();
};

EditorPreferences &get_preferences();

} // namespace Editor::Config

#endif // EDITOR_CONFIG_H
