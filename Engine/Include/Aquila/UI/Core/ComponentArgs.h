#ifndef AQUILA_UI_CORE_COMPONENT_ARGS_H
#define AQUILA_UI_CORE_COMPONENT_ARGS_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/UI/Core/View.h"

namespace Aquila::UI::Core {

struct ComponentArgs {
	std::vector<std::pair<std::string, std::string>> attributes{};
	std::vector<Unique<View>> children{};
};

}

#endif
