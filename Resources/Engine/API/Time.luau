--!strict

local native = require("@time")

--- Game time. It only runs while the game plays: pausing stops it, Play starts it over.
local Time = {}

--- Seconds the current frame took, the same dt on_update gets.
function Time.get_delta(): number
	return native.get_delta()
end

--- Seconds played since Play, the current frame included.
function Time.get_elapsed(): number
	return native.get_elapsed()
end

--- Frames played since Play, 1 on the first one.
function Time.get_frame(): number
	return native.get_frame()
end

return Time
