#include "Aquila/Player/BuildPlan.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Player/BuildLayout.h"
#include "Aquila/Player/BuildManifest.h"

namespace Aquila::Player {

BuildPlan plan_build(const BuildRequest &request) {
	BuildPlan plan;

	if (!request.is_valid()) {
		AQUILA_LOG_ERROR("Build request is not valid");
		plan.errors.emplace_back("Build request validation failed.");
		return plan;
	}

	if (!request.icon_path.empty()) {
		std::string extension = std::filesystem::path(request.icon_path).extension().string();
		std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return std::tolower(c); });
		std::error_code error;
		if (extension != ".png") {
			plan.errors.emplace_back("The icon has to be a .png file: " + request.icon_path);
		} else if (!std::filesystem::is_regular_file(request.icon_path, error)) {
			plan.errors.emplace_back("The icon file does not exist: " + request.icon_path);
		}
		if (!plan.ok()) {
			return plan;
		}
	}

	// sanitize project_name
	std::string folder_name = request.project_name;
	std::ranges::replace(folder_name, ' ', '_');

	BuildLayout layout{};
	layout.root = (std::filesystem::path(request.output_dir) / folder_name).string();

	// setup manifest
	BuildManifest manifest{};
	manifest.version = 1;
	manifest.name = request.project_name;
	manifest.startup_scene = request.startup_scene;
	manifest.window_width = request.window_width;
	manifest.window_height = request.window_height;
	manifest.window_title = request.window_title;
	manifest.maximized = request.maximized;
	manifest.tone_mapper = request.tone_mapper;
	manifest.shadows = request.shadows;

	plan.manifest = std::move(manifest);
	plan.layout = std::move(layout);

	// MakeDirectory root
	plan.steps.push_back(
		BuildStep{ .kind = BuildStepKind::MakeDirectory, .source = "", .target = plan.layout.root, .content = "" });

	// CopyDirectory template_dir -> root
	plan.steps.push_back(BuildStep{
		.kind = BuildStepKind::CopyDirectory,
		.source = request.template_dir,
		.target = plan.layout.root,
		.content = "",
	});

	// Rename root/AQUILA_PLAYER_EXECUTABLE_NAME -> layout.executable(folder_name)
	std::string default_exe_source = (std::filesystem::path(plan.layout.root) / AQUILA_PLAYER_EXECUTABLE_NAME).string();
#ifdef AQUILA_PLATFORM_WINDOWS
	default_exe_source += AQUILA_PLAYER_EXECUTABLE_SUFFIX;
#endif
	plan.steps.push_back(BuildStep{
		.kind = BuildStepKind::Rename,
		.source = default_exe_source,
		.target = plan.layout.executable(folder_name),
		.content = "",
	});

	// CopyDirectory project_dir -> layout.project_dir()
	plan.steps.push_back(BuildStep{
		.kind = BuildStepKind::CopyDirectory,
		.source = request.project_dir,
		.target = plan.layout.project_dir(),
		.content = "",
	});

	// WriteFile write_manifest(manifest) -> layout.manifest_file()
	plan.steps.push_back(BuildStep{
		.kind = BuildStepKind::WriteFile,
		.source = "",
		.target = plan.layout.manifest_file(),
		.content = write_manifest(plan.manifest),
	});

	if (!request.icon_path.empty()) {
		plan.steps.push_back(BuildStep{
			.kind = BuildStepKind::CopyFile,
			.source = request.icon_path,
			.target = plan.layout.icon_file(),
			.content = "",
		});
	}

	return plan;
}

} // namespace Aquila::Player
