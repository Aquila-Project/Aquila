--!strict

local native = require("@time")
local Scheduler = require("@aquila/Internal/Scheduler")

--- Game time. It only runs while the game plays: pausing stops it, Play starts it over.
local Time = {}

--- What after, every and spawn hand back, to stop them early.
export type Timer = Scheduler.Handle

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

--- Calls `callback` once, `seconds` of game time from now (right after the on_update calls of that frame).
function Time.after(seconds: number, callback: () -> ()): Timer
	return Scheduler.after(seconds, callback)
end

--- Calls `callback` every `seconds` of game time until it's cancelled, at most once per frame.
function Time.every(seconds: number, callback: () -> ()): Timer
	return Scheduler.every(seconds, callback)
end

--- Runs `fn` right away as a sequence: a Time.wait inside it pauses only that sequence, the game goes on.
--- Cancelling it stops it where it waits.
---
--- ```luau
--- Time.spawn(function()
---     door:set_visible(false)
---     Time.wait(2)
---     door:set_visible(true)
--- end)
--- ```
function Time.spawn(fn: (...any) -> (), ...: any): Timer
	return Scheduler.spawn(fn, ...)
end

--- Pauses the sequence it's called from for `seconds` of game time. Only works inside Time.spawn.
function Time.wait(seconds: number)
	Scheduler.wait(seconds)
end

return Time
