#ifndef AQUILA_SCRIPTING_SCRIPT_COMPONENT_H
#define AQUILA_SCRIPTING_SCRIPT_COMPONENT_H

#include "Aquila/Foundation/UUID.h"
#include "Aquila/Scene/ComponentRegistry.h"

namespace Aquila::SceneManagement::Components {

/// Another entity, by the UUID in its MetadataComponent, so it survives saving, loading and Play.
/// A null id is "nothing", an id no entity has is a missing reference (the script gets nil).
struct EntityReference {
	Foundation::UUID id = Foundation::UUID::null();

	bool operator==(const EntityReference &other) const { return id == other.id; }
};

/// What one script field holds. Numbers are doubles, like in Luau.
using ScriptFieldValue = std::variant<bool, F64, std::string, Vec3, EntityReference>;

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
/// Aquila.Field declares values the inspector edits per entity (`local speed = Aquila.Field.number("speed", 5)`),
/// `fields` holds the ones changed from the script's defaults.
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
	/// By field name. Only the values the inspector changed: a field that isn't here, or holds another type than the
	/// script declares now, uses the script's default.
	std::map<std::string, ScriptFieldValue> fields{};
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
