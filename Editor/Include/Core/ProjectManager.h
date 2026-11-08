#pragma once

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Core/ProjectSettings.h"

namespace Editor {

struct ProjectInfo {
	std::string name;
	std::string directory;
	std::string native_directory;
	std::string project_file;
	std::vector<std::string> scenes;
	ProjectSettings settings;
	Uint64 created = 0;
	Uint64 last_opened = 0;

	[[nodiscard]] Uint64 last_activity() const { return last_opened != 0 ? last_opened : created; }
};

enum class ProjectTemplate : Uint8 { Empty, Sandbox };

/// Projects live under K_ROOT in the VFS. open() mounts the project at /project and creates its Content folder.
/// Projects opened from anywhere else are mounted under K_EXTERNAL_ROOT and remembered in the Projects folder's
/// external_projects.json, so list() keeps showing them.
class ProjectManager {
  public:
	static constexpr const char *K_ROOT = "/projects";
	static constexpr const char *K_EXTERNAL_ROOT = "/external-projects";

	ProjectManager();

	bool open(const ProjectInfo &project) const;

	/// Creates `native_parent/<name as a file name>` (empty parent = the Projects folder) with its parents. Outside
	/// the Projects folder the project becomes an external one, see add_existing().
	Option<ProjectInfo> create(const std::string &name, ProjectTemplate project_template = ProjectTemplate::Empty,
							   const std::string &native_parent = {});
	/// Empty when the name is fine for that parent folder, otherwise the reason to show the user.
	[[nodiscard]] std::string validate_name(const std::string &name, const std::string &native_parent = {}) const;
	/// Empty when create() can use the folder as a parent, otherwise the reason to show the user.
	[[nodiscard]] static std::string validate_location(const std::string &native_parent);
	/// The native folder create() makes. Empty when the name has nothing usable in a file name.
	[[nodiscard]] std::string target_folder(const std::string &name, const std::string &native_parent = {}) const;
	static const char *template_name(ProjectTemplate project_template);
	static const char *template_description(ProjectTemplate project_template);
	/// Marks it as just opened, for the recent list.
	void touch(ProjectInfo &project) const;
	/// Lists a scene file (relative to the project root, like "Scenes/Level.aqscene") in the project file.
	bool add_scene(ProjectInfo &project, const std::string &scene) const;
	bool rename(ProjectInfo &project, const std::string &name) const;
	/// Writes `project.settings` into its project file.
	bool save_settings(const ProjectInfo &project) const;
	/// Deletes the project folder. Refuses external projects, see forget().
	bool remove(const ProjectInfo &project) const;
	/// A project folder anywhere on disk (native path). One outside the Projects folder is mounted and remembered.
	/// Nullopt when the folder has no project.aqproject.
	Option<ProjectInfo> add_existing(const std::string &native_directory);
	/// Takes an external project off the list. Its files stay where they are.
	bool forget(const ProjectInfo &project);
	[[nodiscard]] static bool is_external(const ProjectInfo &project);
	[[nodiscard]] const std::string &get_native_root() const { return m_native_root; }
	/// Most recently used first.
	[[nodiscard]] std::vector<ProjectInfo> list() const;
	[[nodiscard]] Option<ProjectInfo> load(const std::string &directory) const;

  private:
	[[nodiscard]] std::string to_native(const std::string &directory) const;
	/// The VFS directory of a native project folder that sits directly in the Projects folder, nullopt otherwise.
	[[nodiscard]] Option<std::string> internal_directory(const std::string &native_folder) const;
	[[nodiscard]] std::vector<std::string> read_external() const;
	bool write_external(const std::vector<std::string> &folders) const;
	/// @return its VFS directory
	std::string mount_external(const std::string &native_directory) const;

	std::string m_native_root;
	mutable std::map<std::string, std::string> m_external_mounts;
};

} // namespace Editor
