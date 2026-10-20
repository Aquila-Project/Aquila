#include "Core/ProjectSettings.h"

namespace Editor {

namespace {

template <typename T> void read(const nlohmann::ordered_json &json, const char *key, T &out) {
	if (!json.contains(key)) {
		return;
	}
	try {
		out = json[key].get<T>();
	} catch (const nlohmann::json::exception &) {
	}
}

}

nlohmann::ordered_json project_settings_to_json(const ProjectSettings &settings) {
	return {
		{ "startup_scene", settings.startup_scene },
		{ "icon", settings.icon },
		{ "window_title", settings.window_title },
		{ "window_width", settings.window_width },
		{ "window_height", settings.window_height },
		{ "start_maximized", settings.start_maximized },
		{ "tone_mapper", std::string(Aquila::Rendering::tone_mapper_name(settings.tone_mapper)) },
		{ "shadows", settings.shadows },
		{ "build_output", settings.build_output },
	};
}

ProjectSettings project_settings_from_json(const nlohmann::ordered_json &json) {
	ProjectSettings settings;
	if (!json.is_object()) {
		return settings;
	}
	read(json, "startup_scene", settings.startup_scene);
	read(json, "icon", settings.icon);
	read(json, "window_title", settings.window_title);
	read(json, "window_width", settings.window_width);
	read(json, "window_height", settings.window_height);
	read(json, "start_maximized", settings.start_maximized);
	std::string tone_mapper(Aquila::Rendering::tone_mapper_name(settings.tone_mapper));
	read(json, "tone_mapper", tone_mapper);
	settings.tone_mapper = Aquila::Rendering::tone_mapper_from_name(tone_mapper);
	read(json, "shadows", settings.shadows);
	read(json, "build_output", settings.build_output);
	return settings;
}

Aquila::Rendering::RenderSettings game_render_settings(const ProjectSettings &settings) {
	Aquila::Rendering::RenderSettings render = Aquila::Rendering::RenderSettings::game();
	render.tone_mapper = settings.tone_mapper;
	render.show_shadows = settings.shadows;
	return render;
}

}
