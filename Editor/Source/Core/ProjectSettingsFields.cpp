#include "Core/ProjectSettingsFields.h"

#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Platform/Filesystem/Filesystem.h"

namespace Editor {

using Aquila::Reflection::PropertyHints;
using Aquila::Reflection::TypeBuilder;
using Aquila::Reflection::TypeInfo;

void add_startup_scene_field(TypeInfo &type, const ProjectInfo &project) {
	std::vector<std::string> scenes = { std::string() };
	std::vector<std::string> labels = { "First scene of the project" };
	for (const std::string &scene : project.scenes) {
		scenes.push_back(scene);
		labels.push_back(scene);
	}
	TypeBuilder<ProjectSettings>(type).choice("Startup Scene", &ProjectSettings::startup_scene, std::move(scenes),
											  std::move(labels));
}

void add_icon_field(TypeInfo &type, const ProjectInfo &project) {
	std::vector<std::string> icons = { std::string() };
	const std::string content(Aquila::SharedConstants::PROJECT_CONTENT_FOLDER);
	for (const std::string &png :
		 Aquila::Platform::Filesystem::dir_find_files(project.native_directory + "/" + content, ".png")) {
		icons.push_back(content + "/" + png);
	}
	if (std::ranges::find(icons, project.settings.icon) == icons.end()) {
		icons.push_back(project.settings.icon);
	}
	std::vector<std::string> labels = icons;
	labels.front() = "Aquila logo";
	TypeBuilder<ProjectSettings>(type).choice("Icon", &ProjectSettings::icon, std::move(icons), std::move(labels));
}

void add_window_size_fields(TypeInfo &type) {
	TypeBuilder<ProjectSettings>(type)
		.property("Width", &ProjectSettings::window_width,
				  PropertyHints{ .min = 64.F, .max = 16384.F, .speed = 1.F, .unit = "px" })
		.property("Height", &ProjectSettings::window_height,
				  PropertyHints{ .min = 64.F, .max = 16384.F, .speed = 1.F, .unit = "px" });
}

void add_build_output_fields(TypeInfo &type) {
	TypeBuilder<ProjectSettings>(type)
		.property("Output Folder", &ProjectSettings::build_output)
		.read_only("Default Folder", [](const ProjectSettings &) { return default_build_output(); });
}

}
