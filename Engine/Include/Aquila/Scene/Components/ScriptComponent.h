#ifndef AQUILA_SCRIPTING_SCRIPT_COMPONENT_H
#define AQUILA_SCRIPTING_SCRIPT_COMPONENT_H

#include "Aquila/Scene/ComponentRegistry.h"

namespace Aquila::SceneManagement::Components {

/// Runs the Luau script at `path` (a VFS path) on this entity. Every hook is optional.
///
/// @code
/// local Aquila = require("@aquila")
/// local M = {}
///
/// function M.on_start(self: Entity)
///     Aquila.log("hello from " .. self:get_name())
/// end
///
/// function M.on_update(self: Entity, dt: number)
///     local move = Aquila.Input.get_vector("A", "D", "S", "W")
///     self:set_position(self:get_position() + vector.create(move.x, 0, move.y) * 5 * dt)
/// end
///
/// function M.on_stop(self: Entity)
///     Aquila.log("bye from " .. self:get_name())
/// end
///
/// return M
/// @endcode
///
/// The API is split in modules (Aquila.Input, Aquila.Time, Aquila.Scene, Aquila.Screen, Aquila.Camera, Aquila.Log),
/// written in Luau in Resources/Engine/API over the native ones ScriptBindings.cpp binds. Errors go to the console
/// with a traceback.
///
/// @note Scripts only run while the game plays (Play in the editor, always in a built game). on_start runs
/// again on every Play. on_stop runs when the game stops, or when the script is removed or replaced mid game,
/// not when its entity is destroyed.
struct ScriptComponent {
	std::string path;
};

/// Adds Script to the ComponentRegistry, so the inspector shows it with an asset slot for the path.
inline void register_script_component() {
	auto &registry = SceneManagement::ComponentRegistry::instance();
	if (registry.find("Script") != nullptr) {
		return;
	}
	registry.register_component<ScriptComponent>("Script").property("Path", &ScriptComponent::path,
																	{ .asset_type = "script" });
}

} // namespace Aquila::SceneManagement::Components

#endif
