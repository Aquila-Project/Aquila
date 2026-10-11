--!strict

local native = require("@native")

--- The Aquila engine API: `local Aquila = require("@aquila")`.
local Aquila = {}

--- Writes a line to the engine log.
function Aquila.log(message: string)
	native.log(message)
end

--- Requests another frame. The editor only redraws when something changes, so a script that animates calls this
--- every update.
function Aquila.request_frame()
	native.request_frame()
end

return Aquila
