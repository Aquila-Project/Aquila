#pragma once

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Editor {

struct ProjectInfo {
	std::string name;
	std::string directory;
	std::string native_directory;
	std::string project_file;
	std::vector<std::string> scenes;
	Uint64 created = 0;
	Uint64 last_opened = 0;

	[[nodiscard]] Uint64 last_activity() const { return last_opened != 0 ? last_opened : created; }
};

enum class ProjectTemplate : Uint8 { Empty, Sandbox };

/// Projects live under K_ROOT in the VFS. open() mounts the project at /project and creates its Content folder.
class ProjectManager {
  public:
	static constexpr const char *K_ROOT = "/projects";

	ProjectManager();

	bool open(const ProjectInfo &project) const;

	Option<ProjectInfo> create(const std::string &name, ProjectTemplate project_template = ProjectTemplate::Empty);
	/// Empty when the name is fine, otherwise the reason to show the user.
	[[nodiscard]] std::string validate_name(const std::string &name) const;
	[[nodiscard]] static std::string sanitize(const std::string &name);
	static const char *template_name(ProjectTemplate project_template);
	static const char *template_description(ProjectTemplate project_template);
	/// Marks it as just opened, for the recent list.
	void touch(ProjectInfo &project) const;
	bool rename(ProjectInfo &project, const std::string &name) const;
	bool remove(const ProjectInfo &project) const;
	/// Most recently used first.
	[[nodiscard]] std::vector<ProjectInfo> list() const;
	[[nodiscard]] Option<ProjectInfo> load(const std::string &directory) const;

  private:
	[[nodiscard]] std::string to_native(const std::string &directory) const;

	std::string m_native_root;
};

} // namespace Editor
