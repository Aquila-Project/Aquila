--!strict

local native = require("@native")

--- The engine log, which the editor's console shows.
local Log = {}

function Log.info(message: string)
	native.log(message)
end

function Log.warn(message: string)
	native.warn(message)
end

function Log.error(message: string)
	native.error(message)
end

return Log
