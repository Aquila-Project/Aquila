--!strict

local native = require("@native")

--- The Aquila engine API: `local Aquila = require("@aquila")`. Every area is its own module, and each one can be
--- required on its own too, like `local Input = require("@aquila/Input")` (that's also how you get its types,
--- `Input.Key`).
local Aquila = {}

Aquila.Input = require("@aquila/Input")
Aquila.Time = require("@aquila/Time")
Aquila.Scene = require("@aquila/Scene")
Aquila.Screen = require("@aquila/Screen")
Aquila.Camera = require("@aquila/Camera")
Aquila.Log = require("@aquila/Log")

--- Writes a line to the engine log, same as Aquila.Log.info().
function Aquila.log(message: string)
	native.log(message)
end

--- Requests another frame. Frames already come back to back while the game plays, this is only for the rare
--- script that runs while paused.
function Aquila.request_frame()
	native.request_frame()
end

return Aquila
