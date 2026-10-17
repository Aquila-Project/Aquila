#include "Core/ProjectManager.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Platform/Filesystem/Filesystem.h"
#include "Aquila/Platform/Filesystem/NativeFileSystem.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Components/LightComponent.h"
#include "Aquila/Scene/Components/SkyLightComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/Entity.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Scene/Scene.h"

#include "json.hpp"

namespace Editor {

using Aquila::Platform::Filesystem::NativeFileSystem;
using Aquila::Platform::Filesystem::VirtualFileSystem;
namespace Components = Aquila::SceneManagement::Components;

namespace {

Uint64 now_seconds() {
	const auto now = std::chrono::system_clock::now().time_since_epoch();
	return static_cast<Uint64>(std::chrono::duration_cast<std::chrono::seconds>(now).count());
}

bool update_project_file(const ProjectInfo &project, const std::function<void(nlohmann::ordered_json &)> &edit) {
	auto *vfs = VirtualFileSystem::get();
	nlohmann::ordered_json root =
		nlohmann::ordered_json::parse(vfs->read_text_file(project.project_file), nullptr, false);
	if (root.is_discarded()) {
		return false;
	}
	edit(root);
	return vfs->write_text_file(project.project_file, root.dump(4));
}

void remove_directory(const std::string &directory) {
	auto *vfs = VirtualFileSystem::get();
	for (const std::string &entry : vfs->list_directory(directory)) {
		const std::string path = directory + "/" + entry;
		if (vfs->is_directory(path)) {
			remove_directory(path);
		} else {
			vfs->delete_file_aq(path);
		}
	}
	vfs->delete_directory(directory);
}

std::string trim(const std::string &text) {
	const auto first = text.find_first_not_of(" \t");
	if (first == std::string::npos) {
		return {};
	}
	return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

} // namespace

ProjectManager::ProjectManager() : m_native_root(Aquila::Platform::Filesystem::path_executable_dir() + "/Projects") {
	VirtualFileSystem::get()->mount(K_ROOT, std::make_shared<NativeFileSystem>(m_native_root));
}

std::string ProjectManager::to_native(const std::string &directory) const {
	const std::string_view root = K_ROOT;
	std::string_view relative = directory;
	if (relative.starts_with(root)) {
		relative.remove_prefix(root.size());
	}
	return m_native_root + std::string(relative);
}

bool ProjectManager::open(const ProjectInfo &project) const {
	using namespace Aquila::SharedConstants;
	auto *vfs = VirtualFileSystem::get();

	for (std::string_view folder : { PROJECT_CONTENT_FOLDER, PROJECT_SCENES_FOLDER }) {
		const std::string directory = project.directory + "/" + std::string(folder);
		if (!vfs->is_directory(directory) && !vfs->create_dir(directory)) {
			AQUILA_LOG_ERROR("ProjectManager: failed to create {}", directory);
			return false;
		}
	}

	const std::string mount(PROJECT_MOUNT);
	vfs->unmount(mount);
	if (!vfs->mount(mount, std::make_shared<NativeFileSystem>(project.native_directory))) {
		AQUILA_LOG_ERROR("ProjectManager: failed to mount {} at {}", project.directory, mount);
		return false;
	}

	AQUILA_LOG_INFO("ProjectManager: '{}' mounted at {}", project.name, mount);
	return true;
}

std::string ProjectManager::sanitize(const std::string &name) {
	std::string out;
	out.reserve(name.size());
	for (const char c : name) {
		const auto uc = static_cast<unsigned char>(c);
		if ((std::isalnum(uc) != 0) || c == '-' || c == '_') {
			out += c;
		} else if (c == ' ') {
			out += '_';
		}
	}
	return out;
}

const char *ProjectManager::template_name(ProjectTemplate project_template) {
	switch (project_template) {
	case ProjectTemplate::Sandbox:
		return "Lit sandbox";
	case ProjectTemplate::Empty:
		break;
	}
	return "Empty";
}

const char *ProjectManager::template_description(ProjectTemplate project_template) {
	switch (project_template) {
	case ProjectTemplate::Sandbox:
		return "A camera, a directional sun and a procedural sky, ready to fill with meshes.";
	case ProjectTemplate::Empty:
		break;
	}
	return "A single camera and nothing else. Start from a blank scene.";
}

std::string ProjectManager::validate_name(const std::string &name) const {
	if (trim(name).empty()) {
		return "Enter a project name";
	}
	const std::string safe = sanitize(name);
	if (safe.empty()) {
		return "Use letters, numbers, spaces, - or _";
	}
	if (VirtualFileSystem::get()->exists(std::string(K_ROOT) + "/" + safe + "/project.aqproject")) {
		return "A project with this name already exists";
	}
	return {};
}

bool ProjectManager::add_scene(ProjectInfo &project, const std::string &scene) const {
	if (std::ranges::find(project.scenes, scene) != project.scenes.end()) {
		return true;
	}
	project.scenes.push_back(scene);
	const std::vector<std::string> scenes = project.scenes;
	return update_project_file(project, [&scenes](nlohmann::ordered_json &root) { root["scenes"] = scenes; });
}

void ProjectManager::touch(ProjectInfo &project) const {
	project.last_opened = now_seconds();
	const Uint64 stamp = project.last_opened;
	update_project_file(project, [stamp](nlohmann::ordered_json &root) { root["last_opened"] = stamp; });
}

bool ProjectManager::rename(ProjectInfo &project, const std::string &name) const {
	const std::string clean = trim(name);
	if (clean.empty()) {
		return false;
	}
	if (!update_project_file(project, [&clean](nlohmann::ordered_json &root) { root["name"] = clean; })) {
		return false;
	}
	project.name = clean;
	return true;
}

bool ProjectManager::remove(const ProjectInfo &project) const {
	if (!VirtualFileSystem::get()->exists(project.project_file)) {
		return false;
	}
	remove_directory(project.directory);
	return !VirtualFileSystem::get()->exists(project.directory);
}

Option<ProjectInfo> ProjectManager::create(const std::string &name, ProjectTemplate project_template) {
	auto *vfs = VirtualFileSystem::get();

	const std::string safe = sanitize(name);
	if (safe.empty()) {
		AQUILA_LOG_ERROR("ProjectManager: invalid project name '{}'", name);
		return std::nullopt;
	}

	ProjectInfo info;
	info.name = name;
	info.directory = std::string(K_ROOT) + "/" + safe;
	info.native_directory = to_native(info.directory);
	info.project_file = info.directory + "/project.aqproject";
	info.created = now_seconds();

	const std::string scene_file =
		std::string(Aquila::SharedConstants::PROJECT_SCENES_FOLDER) + "/" + safe + ".aqscene";
	info.scenes = { scene_file };

	if (vfs->exists(info.project_file)) {
		AQUILA_LOG_WARNING("ProjectManager: project '{}' already exists", safe);
		return std::nullopt;
	}

	if (!vfs->create_dir(info.directory)) {
		AQUILA_LOG_ERROR("ProjectManager: failed to create directory {}", info.directory);
		return std::nullopt;
	}
	vfs->create_dir(info.directory + "/" + std::string(Aquila::SharedConstants::PROJECT_CONTENT_FOLDER));
	vfs->create_dir(info.directory + "/" + std::string(Aquila::SharedConstants::PROJECT_SCENES_FOLDER));

	Aquila::SceneManagement::Scene scene(name);
	auto *entity_manager = scene.get_entity_manager();
	auto camera = entity_manager->create_entity("Camera");
	auto &camera_component = camera.add_component<Components::CameraComponent>();
	camera_component.fov = 60.F;
	camera_component.near_plane = 0.1F;
	camera_component.far_plane = 500.F;
	camera_component.aspect_ratio = 16.F / 9.F;
	camera_component.primary = true;
	camera.get_component<Components::TransformComponent>().set_local_position({ 0.F, 1.5F, -5.F });
	scene.set_active_camera(camera);
	if (project_template == ProjectTemplate::Sandbox) {
		auto sun = entity_manager->create_entity("Sun");
		sun.add_component<Components::LightComponent>();
		auto sky = entity_manager->create_entity("Sky");
		sky.add_component<Components::SkyLightComponent>();
	}
	scene.serialize(info.directory + "/" + scene_file);

	nlohmann::ordered_json root;
	root["version"] = 1;
	root["name"] = name;
	root["created"] = info.created;
	root["scenes"] = info.scenes;

	if (!vfs->write_text_file(info.project_file, root.dump(4))) {
		AQUILA_LOG_ERROR("ProjectManager: failed to write {}", info.project_file);
		return std::nullopt;
	}

	AQUILA_LOG_INFO("ProjectManager: created project '{}' at {}", name, info.directory);
	return info;
}

std::vector<ProjectInfo> ProjectManager::list() const {
	std::vector<ProjectInfo> projects;
	auto *vfs = VirtualFileSystem::get();

	for (const std::string &entry : vfs->list_directory(K_ROOT)) {
		const std::string directory = std::string(K_ROOT) + "/" + entry;
		if (!vfs->is_directory(directory)) {
			continue;
		}
		if (Option<ProjectInfo> info = load(directory)) {
			projects.push_back(*info);
		}
	}

	std::ranges::stable_sort(
		projects, [](const ProjectInfo &a, const ProjectInfo &b) { return a.last_activity() > b.last_activity(); });

	return projects;
}

Option<ProjectInfo> ProjectManager::load(const std::string &directory) const {
	auto *vfs = VirtualFileSystem::get();

	const std::string project_file = directory + "/project.aqproject";
	if (!vfs->exists(project_file)) {
		return std::nullopt;
	}

	const std::string text = vfs->read_text_file(project_file);
	nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
	if (root.is_discarded()) {
		AQUILA_LOG_WARNING("ProjectManager: malformed project file {}", project_file);
		return std::nullopt;
	}

	ProjectInfo info;
	info.directory = directory;
	info.native_directory = to_native(directory);
	info.project_file = project_file;
	info.name = root.value("name", std::string());
	info.created = root.value("created", static_cast<Uint64>(0));
	info.last_opened = root.value("last_opened", static_cast<Uint64>(0));
	if (root.contains("scenes") && root["scenes"].is_array()) {
		info.scenes = root["scenes"].get<std::vector<std::string>>();
	}

	return info;
}

} // namespace Editor
