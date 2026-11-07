--!strict

local native = require("@native")

--- Messages between scripts, by name. Every script in the game hears every emit, a new Play starts with no
--- handlers. A handler keeps running after its entity is destroyed, so disconnect it in on_stop.
---
--- ```luau
--- Events.on("damage", function(target: Entity, amount: number)
---     if target:get_id() == self:get_id() then
---         health -= amount
---     end
--- end)
---
--- Events.emit("damage", enemy, 10)
--- ```
local Events = {}

export type Connection = {
	disconnect: (self: Connection) -> (),
}

type Handler = {
	callback: (...any) -> (),
	once: boolean,
	connected: boolean,
}

local handlers: { [string]: { Handler } } = {}

local function connect(name: string, callback: (...any) -> (), once: boolean): Connection
	local handler: Handler = { callback = callback, once = once, connected = true }
	local list = handlers[name]
	if list == nil then
		list = {}
		handlers[name] = list
	end
	table.insert(list, handler)
	return {
		disconnect = function(_: Connection)
			handler.connected = false
		end,
	}
end

--- Calls `handler` with the emit's arguments every time `name` is emitted.
function Events.on(name: string, handler: (...any) -> ()): Connection
	return connect(name, handler, false)
end

--- Like on, for the next emit only.
function Events.once(name: string, handler: (...any) -> ()): Connection
	return connect(name, handler, true)
end

--- Calls every handler of `name` right away, in the order they were added. Handlers added while it runs wait for
--- the next emit. A handler that errors is logged and the others still run.
function Events.emit(name: string, ...: any)
	local list = handlers[name]
	if list == nil then
		return
	end
	local current = table.clone(list)
	for _, handler in current do
		if not handler.connected then
			continue
		end
		if handler.once then
			handler.connected = false
		end
		local ok, message = xpcall(handler.callback :: (...any) -> any, debug.traceback, ...)
		if not ok then
			native.error(tostring(message))
		end
	end
	local kept: { Handler } = {}
	for _, handler in handlers[name] do
		if handler.connected then
			table.insert(kept, handler)
		end
	end
	handlers[name] = kept
end

return Events
