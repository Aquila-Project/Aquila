#ifndef EDITOR_CONFIG_H
#define EDITOR_CONFIG_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Rendering/RenderSettings.h"
#include "Aquila/UI/Core/FontManager.h"

namespace Editor::Config {

constexpr const char *EDITOR_NAME = "Aquila Editor";
constexpr const char *EDITOR_VERSION = AQUILA_VERSION_STRING;
constexpr uint32_t VERSION_MAJOR = AQUILA_VERSION_MAJOR;
constexpr uint32_t VERSION_MINOR = AQUILA_VERSION_MINOR;
constexpr uint32_t VERSION_PATCH = AQUILA_VERSION_PATCH;
constexpr const char *EDITOR_BUILD_DATE = __DATE__;
constexpr const char *EDITOR_TITLE = "Aquila Editor " AQUILA_VERSION_STRING " | " __DATE__;

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

/// What the viewport shows when the editor starts. The Show popover changes the live state, not these.
struct ViewportPreferences {
	bool show_grid = true;
	bool show_sky = true;
	bool show_shadows = true;
	bool show_outline = true;
	bool realtime = false;
	Aquila::Rendering::ToneMapper tone_mapper = Aquila::Rendering::ToneMapper::AgX;
	bool axis_widget = true;
	bool selection_label = true;
	bool safe_frame = false;
	bool render_status = false;
	bool statistics = false;
	bool gpu_timings = false;

	bool operator==(const ViewportPreferences &) const = default;
};

/// The editor camera. `fly_speed` is m/s, `zoom_step` meters per scroll notch, `fov` vertical degrees.
struct NavigationPreferences {
	F32 fly_speed = 5.F;
	F32 look_sensitivity = 1.F;
	bool invert_y = false;
	F32 zoom_step = 0.15F;
	F32 fov = 60.F;

	bool operator==(const NavigationPreferences &) const = default;
};

/// Snap steps are meters, degrees and scale units. `gizmo_size` is the axis length in pixels.
struct GizmoPreferences {
	bool snapping = false;
	F32 translate_snap = 0.5F;
	F32 rotate_snap = 15.F;
	F32 scale_snap = 0.1F;
	F32 gizmo_size = 96.F;

	bool operator==(const GizmoPreferences &) const = default;
};

struct PlayPreferences {
	/// Play switches to the Game workspace unless a Game view is already on screen.
	bool switch_to_game_view = true;
	/// Stop goes back to the workspace that was open before Play.
	bool return_after_stop = true;

	bool operator==(const PlayPreferences &) const = default;
};

struct ContentBrowserPreferences {
	/// Pixels, the longest side of a tile's picture.
	F32 thumbnail_size = 48.F;
	bool show_extensions = true;

	bool operator==(const ContentBrowserPreferences &) const = default;
};

struct StartupPreferences {
	/// Skip the launcher and open the most recently opened project.
	bool reopen_last_project = false;

	bool operator==(const StartupPreferences &) const = default;
};

struct AdvancedPreferences {
#ifdef AQUILA_DEBUG
	bool shader_hot_reload = true;
#else
	bool shader_hot_reload = false;
#endif

	bool operator==(const AdvancedPreferences &) const = default;
};

/// Saved as JSON at preference_file_path, one object per group. Missing keys keep their defaults.
struct EditorPreferences {
	FontSettings fonts;
	UISettings ui;
	F32 ui_scale = 1.0F;

	ViewportPreferences viewport;
	NavigationPreferences navigation;
	GizmoPreferences gizmos;
	PlayPreferences play;
	ContentBrowserPreferences content_browser;
	StartupPreferences startup;
	AdvancedPreferences advanced;

	std::map<std::string, std::vector<std::string>> shortcuts;

	std::string preference_file_path = "/app/editor_preferences.json";

	void load_from_file();
	void save_to_file() const;
	void reset_to_defaults();
};

EditorPreferences &get_preferences();

} // namespace Editor::Config

#endif // EDITOR_CONFIG_H
