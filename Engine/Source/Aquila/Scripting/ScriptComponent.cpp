#include "Aquila/Scripting/ScriptComponent.h"

#include "Aquila/Scene/ComponentRegistry.h"

namespace Aquila::Scripting {

void register_script_component() {
	auto &registry = SceneManagement::ComponentRegistry::instance();
	if (registry.find("Script") != nullptr) {
		return;
	}
	registry.register_component<ScriptComponent>("Script").property("Path", &ScriptComponent::path,
																	{ .asset_type = "script" });
}

} // namespace Aquila::Scripting
