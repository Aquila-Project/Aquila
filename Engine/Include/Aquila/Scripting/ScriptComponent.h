#ifndef AQUILA_SCRIPTING_SCRIPT_COMPONENT_H
#define AQUILA_SCRIPTING_SCRIPT_COMPONENT_H

namespace Aquila::Scripting {

/// Runs the Luau script at `path` (a VFS path) on this entity. Both hooks are optional.
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
///     self:set_position(self:get_position() + vector.create(0, dt, 0))
///     Aquila.request_frame()
/// end
///
/// return M
/// @endcode
///
/// @note on_update only runs on rendered frames. A script that animates has to call
/// Aquila.request_frame() every update, or the editor goes idle and it stops.
struct ScriptComponent {
	std::string path;
};

/// Adds Script to the ComponentRegistry, so the inspector shows it with an asset slot for the path.
void register_script_component();

} // namespace Aquila::Scripting

#endif
