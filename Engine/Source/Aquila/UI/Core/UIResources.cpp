#include "Aquila/UI/Core/UIResources.h"

namespace Aquila::UI::Core {

std::string engine_component_file(std::string_view name) {
	return std::string(UI_COMPONENTS_DIR) + std::string(name) + ".aqlayout";
}

}
