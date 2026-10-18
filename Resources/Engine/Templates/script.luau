--!strict

local Aquila = require("@aquila")

local M = {}

function M.on_start(self: Entity)
	Aquila.log(self:get_name() .. " started")
end

function M.on_update(self: Entity, dt: number) end

function M.on_stop(self: Entity) end

return M
