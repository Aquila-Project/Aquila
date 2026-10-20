#ifndef AQUILA_PLAYER_BUILD_PLAN_H
#define AQUILA_PLAYER_BUILD_PLAN_H

#include <algorithm>

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Player/BuildLayout.h"
#include "Aquila/Player/BuildManifest.h"

namespace Aquila::Player {

struct BuildRequest {
	std::string project_name;
	std::string project_dir;
	std::string startup_scene;
	std::string template_dir;
	std::string output_dir;
	Uint32 window_width = 1280;
	Uint32 window_height = 720;
	/// Native path to a PNG for the game's window icon. Empty uses the Aquila logo.
	std::string icon_path;
	/// The project name when empty.
	std::string window_title;
	bool maximized = false;
	Rendering::ToneMapper tone_mapper = Rendering::ToneMapper::AgX;
	bool shadows = true;

	[[nodiscard]] bool is_valid() const {
		if (project_name.empty() || project_dir.empty() || template_dir.empty() || output_dir.empty()) {
			return false;
		}

		constexpr std::string_view suffix = ".aqscene";
		if (startup_scene.length() < suffix.length() ||
			startup_scene.compare(startup_scene.length() - suffix.length(), suffix.length(), suffix) != 0) {
			return false;
		}

		namespace fs = std::filesystem;
		try {
			fs::path out = fs::absolute(output_dir).lexically_normal();
			fs::path proj = fs::absolute(project_dir).lexically_normal();
			fs::path tmpl = fs::absolute(template_dir).lexically_normal();

			auto is_subpath = [](const fs::path &base, const fs::path &target) {
				auto [b_it, t_it] = std::ranges::mismatch(base, target);
				return b_it == base.end();
			};

			if (is_subpath(proj, out) || is_subpath(tmpl, out)) {
				return false;
			}
		} catch (...) {
			return false;
		}

		return true;
	}
};

enum class BuildStepKind : Uint8 { MakeDirectory, CopyDirectory, Rename, WriteFile, CopyFile };

struct BuildStep {
	BuildStepKind kind = BuildStepKind::MakeDirectory;
	std::string source;
	std::string target;
	std::string content;
};

struct BuildPlan {
	BuildLayout layout;
	BuildManifest manifest;
	std::vector<BuildStep> steps;
	std::vector<std::string> errors;

	[[nodiscard]] bool ok() const { return errors.empty(); }
};

[[nodiscard]] BuildPlan plan_build(const BuildRequest &request);

} // namespace Aquila::Player

#endif
