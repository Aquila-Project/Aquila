#include "Core/EditorConfig.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"

#include "json.hpp"

namespace Editor::Config {

namespace {

EditorPreferences s_preferences;

template <typename Visit> void visit(ViewportPreferences &p, Visit &&field) {
	field("show_grid", p.show_grid);
	field("show_sky", p.show_sky);
	field("show_shadows", p.show_shadows);
	field("show_outline", p.show_outline);
	field("realtime", p.realtime);
	field("tone_mapper", p.tone_mapper);
	field("axis_widget", p.axis_widget);
	field("selection_label", p.selection_label);
	field("safe_frame", p.safe_frame);
	field("render_status", p.render_status);
	field("statistics", p.statistics);
	field("gpu_timings", p.gpu_timings);
}

template <typename Visit> void visit(NavigationPreferences &p, Visit &&field) {
	field("fly_speed", p.fly_speed);
	field("look_sensitivity", p.look_sensitivity);
	field("invert_y", p.invert_y);
	field("zoom_step", p.zoom_step);
	field("fov", p.fov);
}

template <typename Visit> void visit(GizmoPreferences &p, Visit &&field) {
	field("snapping", p.snapping);
	field("translate_snap", p.translate_snap);
	field("rotate_snap", p.rotate_snap);
	field("scale_snap", p.scale_snap);
	field("gizmo_size", p.gizmo_size);
}

template <typename Visit> void visit(PlayPreferences &p, Visit &&field) {
	field("switch_to_game_view", p.switch_to_game_view);
	field("return_after_stop", p.return_after_stop);
}

template <typename Visit> void visit(ContentBrowserPreferences &p, Visit &&field) {
	field("thumbnail_size", p.thumbnail_size);
	field("show_extensions", p.show_extensions);
}

template <typename Visit> void visit(StartupPreferences &p, Visit &&field) {
	field("reopen_last_project", p.reopen_last_project);
}

template <typename Visit> void visit(AdvancedPreferences &p, Visit &&field) {
	field("shader_hot_reload", p.shader_hot_reload);
}

template <typename Visit> void visit(FontSettings &p, Visit &&field) {
	field("main_family", p.main_family);
	field("mono_family", p.mono_family);
	field("size", p.size);
}

template <typename T> void read_value(const nlohmann::json &value, T &out) {
	try {
		out = value.get<T>();
	} catch (const nlohmann::json::exception &) {
	}
}

void read_value(const nlohmann::json &value, Aquila::Rendering::ToneMapper &out) {
	if (value.is_string()) {
		out = Aquila::Rendering::tone_mapper_from_name(value.get<std::string>());
	}
}

template <typename T> nlohmann::ordered_json write_value(const T &value) {
	return value;
}

nlohmann::ordered_json write_value(const Aquila::Rendering::ToneMapper &value) {
	return std::string(Aquila::Rendering::tone_mapper_name(value));
}

template <typename Group> void read_group(const nlohmann::json &root, const char *key, Group &group) {
	if (!root.contains(key) || !root[key].is_object()) {
		return;
	}
	const nlohmann::json &object = root[key];
	visit(group, [&object](const char *name, auto &field) {
		if (object.contains(name)) {
			read_value(object[name], field);
		}
	});
}

template <typename Group> void write_group(nlohmann::ordered_json &root, const char *key, const Group &group) {
	nlohmann::ordered_json object = nlohmann::ordered_json::object();
	visit(const_cast<Group &>(group),
		  [&object](const char *name, const auto &field) { object[name] = write_value(field); });
	root[key] = std::move(object);
}

}

EditorPreferences &get_preferences() {
	return s_preferences;
}

void EditorPreferences::load_from_file() {
	auto *vfs = Aquila::Platform::Filesystem::VirtualFileSystem::get();

	reset_to_defaults();

	if (vfs == nullptr || !vfs->exists(preference_file_path)) {
		AQUILA_LOG_INFO("No editor preferences at {}, using defaults", preference_file_path);
		return;
	}

	const std::string text = vfs->read_text_file(preference_file_path);
	nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
	if (root.is_discarded() || !root.is_object()) {
		AQUILA_LOG_WARNING("Editor preferences at {} are malformed, using defaults", preference_file_path);
		return;
	}

	read_group(root, "fonts", fonts);
	if (root.contains("ui_scale")) {
		read_value(root["ui_scale"], ui_scale);
	}
	read_group(root, "viewport", viewport);
	read_group(root, "navigation", navigation);
	read_group(root, "gizmos", gizmos);
	read_group(root, "play", play);
	read_group(root, "content_browser", content_browser);
	read_group(root, "startup", startup);
	read_group(root, "advanced", advanced);

	if (root.contains("shortcuts") && root["shortcuts"].is_object()) {
		for (const auto &[command, chords] : root["shortcuts"].items()) {
			std::vector<std::string> &target = shortcuts[command];
			if (chords.is_string()) {
				target.push_back(chords.get<std::string>());
			} else if (chords.is_array()) {
				for (const auto &chord : chords) {
					if (chord.is_string()) {
						target.push_back(chord.get<std::string>());
					}
				}
			} else {
				AQUILA_LOG_WARNING("Editor preferences: shortcut '{}' must be a string or a list of strings", command);
			}
		}
	}

	AQUILA_LOG_INFO("Editor preferences loaded from {}", preference_file_path);
}

void EditorPreferences::save_to_file() const {
	nlohmann::ordered_json root;
	write_group(root, "fonts", fonts);
	root["ui_scale"] = ui_scale;
	write_group(root, "viewport", viewport);
	write_group(root, "navigation", navigation);
	write_group(root, "gizmos", gizmos);
	write_group(root, "play", play);
	write_group(root, "content_browser", content_browser);
	write_group(root, "startup", startup);
	write_group(root, "advanced", advanced);
	if (!shortcuts.empty()) {
		root["shortcuts"] = shortcuts;
	}

	auto *vfs = Aquila::Platform::Filesystem::VirtualFileSystem::get();
	if (vfs == nullptr || !vfs->write_text_file(preference_file_path, root.dump(4))) {
		AQUILA_LOG_ERROR("Failed to save editor preferences to {}", preference_file_path);
	}
}

void EditorPreferences::reset_to_defaults() {
	const std::string path = preference_file_path;
	*this = EditorPreferences{};
	preference_file_path = path;
}

} // namespace Editor::Config
