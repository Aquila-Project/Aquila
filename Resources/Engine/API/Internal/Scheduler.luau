--!strict

local native = require("@native")

--- What Aquila.Time's timers and sequences run on. The engine steps it once per frame, after every on_update.
--- Scripts use Aquila.Time, not this.
local Scheduler = {}

export type Handle = {
	cancel: (self: Handle) -> (),
}

type Entry = {
	due: number,
	interval: number?,
	callback: (() -> ())?,
	thread: thread?,
	cancelled: boolean,
}

local now = 0
local entries: { Entry } = {}
local sequences: { [thread]: Entry } = {}

local function handle(entry: Entry): Handle
	return {
		cancel = function(_: Handle)
			entry.cancelled = true
		end,
	}
end

local function resume(thread: thread, ...: any)
	local ok, message = coroutine.resume(thread, ...)
	if not ok then
		native.error(debug.traceback(thread, tostring(message)))
	end
	if coroutine.status(thread) == "dead" then
		sequences[thread] = nil
	end
end

function Scheduler.after(seconds: number, callback: () -> ()): Handle
	local entry: Entry = { due = now + seconds, callback = callback, cancelled = false }
	table.insert(entries, entry)
	return handle(entry)
end

function Scheduler.every(seconds: number, callback: () -> ()): Handle
	assert(seconds > 0, "Time.every needs more than 0 seconds")
	local entry: Entry = { due = now + seconds, interval = seconds, callback = callback, cancelled = false }
	table.insert(entries, entry)
	return handle(entry)
end

function Scheduler.spawn(fn: (...any) -> (), ...: any): Handle
	local thread = coroutine.create(fn)
	local entry: Entry = { due = math.huge, thread = thread, cancelled = false }
	sequences[thread] = entry
	resume(thread, ...)
	return handle(entry)
end

function Scheduler.wait(seconds: number)
	local entry = sequences[coroutine.running()]
	if entry == nil then
		error("Time.wait only works inside Time.spawn", 2)
	end
	entry.due = now + seconds
	table.insert(entries, entry)
	coroutine.yield()
end

function Scheduler.step(dt: number)
	now += dt
	local due: { Entry } = {}
	local waiting: { Entry } = {}
	for _, entry in entries do
		if entry.cancelled then
			if entry.thread then
				sequences[entry.thread] = nil
			end
		elseif entry.due <= now then
			table.insert(due, entry)
		else
			table.insert(waiting, entry)
		end
	end
	entries = waiting

	for _, entry in due do
		if entry.cancelled then
			continue
		end
		local thread = entry.thread
		local callback = entry.callback
		if thread then
			entry.due = math.huge
			resume(thread)
		elseif callback then
			local interval = entry.interval
			if interval then
				entry.due = math.max(entry.due + interval, now)
				table.insert(entries, entry)
			end
			local ok, message = xpcall(callback :: () -> any, debug.traceback)
			if not ok then
				native.error(tostring(message))
			end
		end
	end
end

return Scheduler
