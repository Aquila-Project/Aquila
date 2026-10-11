#ifndef AQUILA_SCRIPTING_SCRIPT_COMPONENT_H
#define AQUILA_SCRIPTING_SCRIPT_COMPONENT_H

#include <string>

namespace Aquila::Scripting {

struct ScriptComponent {
	std::string path;
};

void register_script_component();

} // namespace Aquila::Scripting

#endif
