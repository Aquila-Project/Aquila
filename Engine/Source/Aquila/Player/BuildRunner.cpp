#include "Aquila/Player/BuildRunner.h"
#include "Aquila/Foundation/Macros.h"

namespace Aquila::Player {

BuildResult run_build(const BuildPlan &plan, const BuildProgressCallback &on_progress) {
	BuildResult result;
	result.success = false;

	if (!plan.ok()) {
		AQUILA_LOG_ERROR("Cannot run build: Plan contains errors.");
		result.errors = plan.errors;
		result.errors.emplace_back("Plan contains errors.");
		return result;
	}

	namespace fs = std::filesystem;

	if (fs::exists(plan.layout.root)) {
		fs::path manifest_path = plan.layout.manifest_file();
		if (fs::exists(manifest_path)) {
			std::error_code ec;
			fs::remove_all(plan.layout.root, ec);
			if (ec) {
				std::string err = "Failed to clean existing build directory: " + ec.message();
				AQUILA_LOG_ERROR("{}", err);
				result.errors.push_back(err);
				return result;
			}
		} else {
			std::string err =
				"Root directory exists and does not contain a valid build manifest. Aborting to protect data.";
			AQUILA_LOG_ERROR("{}", err);
			result.errors.push_back(err);
			return result;
		}
	}

	Usize total_steps = plan.steps.size();
	for (Usize i = 0; i < total_steps; ++i) {
		const auto &step = plan.steps[i];

		if (on_progress) {
			std::string step_name;
			switch (step.kind) {
			case BuildStepKind::MakeDirectory:
				step_name = "Creating directory: " + step.target;
				break;
			case BuildStepKind::CopyDirectory:
				step_name = "Copying files to: " + step.target;
				break;
			case BuildStepKind::Rename:
				step_name = "Renaming executable: " + step.target;
				break;
			case BuildStepKind::WriteFile:
				step_name = "Writing manifest file: " + step.target;
				break;
			case BuildStepKind::CopyFile:
				step_name = "Copying file: " + step.target;
				break;
			}

			BuildProgress progress{ .step = i + 1, .step_count = total_steps, .message = std::move(step_name) };
			on_progress(progress);
		}

		std::error_code ec;

		switch (step.kind) {
		case BuildStepKind::MakeDirectory: {
			fs::create_directories(step.target, ec);
			if (ec) {
				std::string err = "Step failed [MakeDirectory]: " + ec.message();
				AQUILA_LOG_ERROR("{}", err);
				result.errors.push_back(err);
				return result;
			}
			break;
		}
		case BuildStepKind::CopyDirectory: {
			if (!fs::exists(step.source)) {
				std::string err = "Step failed [CopyDirectory]: Source path does not exist: " + step.source;
				AQUILA_LOG_ERROR("{}", err);
				result.errors.push_back(err);
				return result;
			}
			fs::create_directories(step.target, ec);
			fs::copy(step.source, step.target, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
			if (ec) {
				std::string err = "Step failed [CopyDirectory]: " + ec.message();
				AQUILA_LOG_ERROR("{}", err);
				result.errors.push_back(err);
				return result;
			}
			break;
		}
		case BuildStepKind::Rename: {
			fs::create_directories(fs::path(step.target).parent_path(), ec);
			fs::rename(step.source, step.target, ec);
			if (ec) {
				std::string err = "Step failed [Rename]: " + ec.message();
				AQUILA_LOG_ERROR("{}", err);
				result.errors.push_back(err);
				return result;
			}
			result.executable = step.target;

#ifndef _WIN32
			if (step.target.ends_with(AQUILA_PLAYER_EXECUTABLE_SUFFIX) ||
				step.target.find(AQUILA_PLAYER_EXECUTABLE_NAME) != std::string::npos) {
				fs::permissions(step.target, fs::perms::owner_exec, fs::perm_options::add, ec);
				if (ec) {
					AQUILA_LOG_WARNING("Failed to set executable permissions: {}", ec.message());
				}
			}
#endif
			break;
		}
		case BuildStepKind::WriteFile: {
			fs::create_directories(fs::path(step.target).parent_path(), ec);
			std::ofstream ofs(step.target, std::ios::binary);
			if (!ofs.is_open()) {
				std::string err = "Step failed [WriteFile]: Could not open file for writing: " + step.target;
				AQUILA_LOG_ERROR("{}", err);
				result.errors.push_back(err);
				return result;
			}
			ofs.write(step.content.data(), static_cast<std::streamsize>(step.content.size()));
			ofs.close();
			if (!ofs) {
				std::string err = "Step failed [WriteFile]: Error writing content to " + step.target;
				AQUILA_LOG_ERROR("{}", err);
				result.errors.push_back(err);
				return result;
			}
			break;
		}
		case BuildStepKind::CopyFile: {
			fs::create_directories(fs::path(step.target).parent_path(), ec);
			fs::copy_file(step.source, step.target, fs::copy_options::overwrite_existing, ec);
			if (ec) {
				std::string err = "Step failed [CopyFile]: " + ec.message();
				AQUILA_LOG_ERROR("{}", err);
				result.errors.push_back(err);
				return result;
			}
			break;
		}
		}
	}

	result.success = true;
	return result;
}

} // namespace Aquila::Player