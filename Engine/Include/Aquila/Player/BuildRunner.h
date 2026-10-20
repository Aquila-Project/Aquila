#ifndef AQUILA_PLAYER_BUILD_RUNNER_H
#define AQUILA_PLAYER_BUILD_RUNNER_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Player/BuildPlan.h"

namespace Aquila::Player {

struct BuildProgress {
	Usize step = 0;
	Usize step_count = 0;
	std::string message;
};

struct BuildResult {
	bool success = false;
	std::string executable;
	std::vector<std::string> errors;
};

using BuildProgressCallback = Delegate<void(const BuildProgress &)>;

[[nodiscard]] BuildResult run_build(const BuildPlan &plan, const BuildProgressCallback &on_progress = {});

}

#endif
