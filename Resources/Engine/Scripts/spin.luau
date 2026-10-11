--!strict

local Aquila = require("@aquila")

local M = {}

local angle = 0

function M.on_start(self: Entity)
	Aquila.log("spinning " .. self:get_name())
end

function M.on_update(self: Entity, dt: number)
	angle += 90 * dt
	self:set_rotation(vector.create(0, angle, 0))
	Aquila.request_frame()
end

return M
