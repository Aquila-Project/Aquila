#include "Core/ProjectManager.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/Platform/Filesystem/Filesystem.h"
#include "Aquila/Platform/Filesystem/NativeFileSystem.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Scene/Components/LightComponent.h"
#include "Aquila/Scene/Components/SkyLightComponent.h"
#include "Aquila/Scene/DefaultScenes.h"
#include "Aquila/Scene/Entity.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Scene/Scene.h"

#include "json.hpp"

namespace Editor {

using Aquila::Platform::Filesystem::NativeFileSystem;
using Aquila::Platform::Filesystem::VirtualFileSystem;
namespace Components = Aquila::SceneManagement::Components;

namespace {

constexpr const char *K_EXTERNAL_FILE = "external_projects.json";

std::string normalized_folder(const std::string &native_directory) {
	std::error_code error;
	std::filesystem::path path = std::filesystem::weakly_canonical(native_directory, error);
	if (error) {
		path = std::filesystem::path(native_directory).lexically_normal();
	}
	std::string folder = path.string();
	while (folder.size() > 1 && (folder.back() == '/' || folder.back() == '\\')) {
		folder.pop_back();
	}
	return folder;
}

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

} // namespace

ProjectManager::ProjectManager() : m_native_root(Aquila::Platform::Filesystem::path_executable_dir() + "/Projects") {
	VirtualFileSystem::get()->mount(K_ROOT, std::make_shared<NativeFileSystem>(m_native_root));
}

std::string ProjectManager::to_native(const std::string &directory) const {
	if (directory.starts_with(K_EXTERNAL_ROOT)) {
		const auto it = m_external_mounts.find(directory);
		return it != m_external_mounts.end() ? it->second : std::string{};
	}
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

std::string ProjectManager::validate_name(const std::string &name, const std::string &native_parent) const {
	if (Aquila::Foundation::trim(name).empty()) {
		return "Enter a project name.";
	}
	const std::string folder = target_folder(name, native_parent);
	if (folder.empty()) {
		return "Use letters, numbers, spaces, - or _.";
	}
	std::error_code error;
	if (std::filesystem::exists(std::filesystem::path(folder) / "project.aqproject", error)) {
		return "A project with this name already exists there.";
	}
	if (std::filesystem::exists(folder, error) &&
		(!std::filesystem::is_directory(folder, error) || !std::filesystem::is_empty(folder, error))) {
		return "Something with this name already exists there.";
	}
	return {};
}

std::string ProjectManager::validate_location(const std::string &native_parent) {
	if (Aquila::Foundation::trim(native_parent).empty()) {
		return "Choose where the project goes.";
	}
	const std::filesystem::path parent(native_parent);
	if (!parent.is_absolute()) {
		return "Use a full path.";
	}
	std::error_code error;
	if (std::filesystem::exists(parent, error) && !std::filesystem::is_directory(parent, error)) {
		return "That is a file, not a folder.";
	}
	return {};
}

std::string ProjectManager::target_folder(const std::string &name, const std::string &native_parent) const {
	const std::string safe = Aquila::Foundation::to_file_name(name);
	if (safe.empty()) {
		return {};
	}
	const std::string parent = native_parent.empty() ? m_native_root : native_parent;
	return normalized_folder((std::filesystem::path(parent) / safe).string());
}

Option<std::string> ProjectManager::internal_directory(const std::string &native_folder) const {
	const std::filesystem::path relative =
		std::filesystem::path(normalized_folder(native_folder)).lexically_relative(normalized_folder(m_native_root));
	if (relative.empty() || relative == "." || std::distance(relative.begin(), relative.end()) != 1 ||
		*relative.begin() == "..") {
		return std::nullopt;
	}
	return std::string(K_ROOT) + "/" + relative.generic_string();
}

bool ProjectManager::add_scene(ProjectInfo &project, const std::string &scene) const {
	if (std::ranges::find(project.scenes, scene) != project.scenes.end()) {
		return true;
	}
	project.scenes.push_back(scene);
	const std::vector<std::string> scenes = project.scenes;
	return update_project_file(project, [&scenes](nlohmann::ordered_json &root) { root["scenes"] = scenes; });
}

bool ProjectManager::save_settings(const ProjectInfo &project) const {
	const nlohmann::ordered_json settings = project_settings_to_json(project.settings);
	return update_project_file(project, [&settings](nlohmann::ordered_json &root) { root["settings"] = settings; });
}

void ProjectManager::touch(ProjectInfo &project) const {
	project.last_opened = now_seconds();
	const Uint64 stamp = project.last_opened;
	update_project_file(project, [stamp](nlohmann::ordered_json &root) { root["last_opened"] = stamp; });
}

bool ProjectManager::rename(ProjectInfo &project, const std::string &name) const {
	const std::string clean(Aquila::Foundation::trim(name));
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
	if (is_external(project) || !VirtualFileSystem::get()->exists(project.project_file)) {
		return false;
	}
	VirtualFileSystem::get()->delete_directory(project.directory, true);
	return !VirtualFileSystem::get()->exists(project.directory);
}

Option<ProjectInfo> ProjectManager::create(const std::string &name, ProjectTemplate project_template,
										   const std::string &native_parent) {
	auto *vfs = VirtualFileSystem::get();

	const std::string safe = Aquila::Foundation::to_file_name(name);
	const std::string folder = target_folder(name, native_parent);
	if (folder.empty()) {
		AQUILA_LOG_ERROR("ProjectManager: invalid project name '{}'", name);
		return std::nullopt;
	}

	ProjectInfo info;
	info.name = name;
	const Option<std::string> internal = internal_directory(folder);
	if (internal) {
		info.directory = *internal;
	} else {
		std::error_code error;
		std::filesystem::create_directories(folder, error);
		if (error) {
			AQUILA_LOG_ERROR("ProjectManager: failed to create {}: {}", folder, error.message());
			return std::nullopt;
		}
		info.directory = mount_external(folder);
	}
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

	if (!vfs->is_directory(info.directory) && !vfs->create_dir(info.directory)) {
		AQUILA_LOG_ERROR("ProjectManager: failed to create directory {}", info.directory);
		return std::nullopt;
	}
	vfs->create_dir(info.directory + "/" + std::string(Aquila::SharedConstants::PROJECT_CONTENT_FOLDER));
	vfs->create_dir(info.directory + "/" + std::string(Aquila::SharedConstants::PROJECT_SCENES_FOLDER));

	Aquila::SceneManagement::Scene scene(name);
	Aquila::SceneManagement::spawn_default_camera(scene, 16.F / 9.F);
	auto *entity_manager = scene.get_entity_manager();
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

	if (!internal) {
		std::vector<std::string> folders = read_external();
		if (std::ranges::find(folders, folder) == folders.end()) {
			folders.push_back(folder);
			write_external(folders);
		}
	}

	AQUILA_LOG_INFO("ProjectManager: created project '{}' at {}", name, info.native_directory);
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
	for (const std::string &folder : read_external()) {
		if (Option<ProjectInfo> info = load(mount_external(folder))) {
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
	if (root.contains("settings")) {
		info.settings = project_settings_from_json(nlohmann::ordered_json::parse(root["settings"].dump()));
	}
	if (root.contains("scenes") && root["scenes"].is_array()) {
		info.scenes = root["scenes"].get<std::vector<std::string>>();
	}

	return info;
}

Option<ProjectInfo> ProjectManager::add_existing(const std::string &native_directory) {
	const std::string folder = normalized_folder(native_directory);
	if (const Option<std::string> internal = internal_directory(folder)) {
		return load(*internal);
	}

	const std::string directory = mount_external(folder);
	Option<ProjectInfo> project = load(directory);
	if (!project) {
		VirtualFileSystem::get()->unmount(directory);
		m_external_mounts.erase(directory);
		return std::nullopt;
	}
	std::vector<std::string> folders = read_external();
	if (std::ranges::find(folders, folder) == folders.end()) {
		folders.push_back(folder);
		write_external(folders);
	}
	return project;
}

bool ProjectManager::forget(const ProjectInfo &project) {
	if (!is_external(project)) {
		return false;
	}
	std::vector<std::string> folders = read_external();
	std::erase(folders, normalized_folder(project.native_directory));
	if (!write_external(folders)) {
		return false;
	}
	VirtualFileSystem::get()->unmount(project.directory);
	m_external_mounts.erase(project.directory);
	return true;
}

bool ProjectManager::is_external(const ProjectInfo &project) {
	return project.directory.starts_with(K_EXTERNAL_ROOT);
}

std::vector<std::string> ProjectManager::read_external() const {
	std::ifstream file(std::filesystem::path(m_native_root) / K_EXTERNAL_FILE);
	if (!file) {
		return {};
	}
	const nlohmann::json root = nlohmann::json::parse(file, nullptr, false);
	if (root.is_discarded() || !root.is_array()) {
		AQUILA_LOG_WARNING("ProjectManager: malformed {}", K_EXTERNAL_FILE);
		return {};
	}
	std::vector<std::string> folders;
	for (const nlohmann::json &entry : root) {
		if (entry.is_string()) {
			folders.push_back(entry.get<std::string>());
		}
	}
	return folders;
}

bool ProjectManager::write_external(const std::vector<std::string> &folders) const {
	std::error_code error;
	std::filesystem::create_directories(m_native_root, error);
	std::ofstream file(std::filesystem::path(m_native_root) / K_EXTERNAL_FILE, std::ios::trunc);
	if (!file) {
		AQUILA_LOG_ERROR("ProjectManager: can't write {} in {}", K_EXTERNAL_FILE, m_native_root);
		return false;
	}
	file << nlohmann::json(folders).dump(4);
	return static_cast<bool>(file);
}

std::string ProjectManager::mount_external(const std::string &native_directory) const {
	const std::string directory =
		std::format("{}/{:016x}", K_EXTERNAL_ROOT, std::hash<std::string>{}(native_directory));
	auto *vfs = VirtualFileSystem::get();
	if (!vfs->is_mounted(directory)) {
		vfs->mount(directory, std::make_shared<NativeFileSystem>(native_directory));
	}
	m_external_mounts[directory] = native_directory;
	return directory;
}

} // namespace Editor
