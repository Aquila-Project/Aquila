#pragma once

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Editor {

struct ProjectInfo {
	std::string name;
	std::string directory;
	std::string project_file;
	std::vector<std::string> scenes;
	Uint64 created = 0;
	Uint64 last_opened = 0;

	[[nodiscard]] Uint64 last_activity() const { return last_opened != 0 ? last_opened : created; }
};

enum class ProjectTemplate : Uint8 { Empty, Sandbox };

class ProjectManager {
  public:
	static constexpr const char *K_ROOT = "/projects";

	ProjectManager();

	Option<ProjectInfo> create(const std::string &name, ProjectTemplate project_template = ProjectTemplate::Empty);
	[[nodiscard]] std::string validate_name(const std::string &name) const;
	[[nodiscard]] static std::string sanitize(const std::string &name);
	static const char *template_name(ProjectTemplate project_template);
	static const char *template_description(ProjectTemplate project_template);
	void touch(ProjectInfo &project) const;
	bool rename(ProjectInfo &project, const std::string &name) const;
	bool remove(const ProjectInfo &project) const;
	[[nodiscard]] std::vector<ProjectInfo> list() const;
	[[nodiscard]] Option<ProjectInfo> load(const std::string &directory) const;
};

} // namespace Editor
