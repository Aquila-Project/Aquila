--!strict

local native = require("@fields")

--- Values the inspector sets per entity, like Unity's serialized fields or Godot's @export. Declare them at the top
--- of the script: each call returns this entity's value (the default until it's changed in the inspector) and
--- shows the field in the inspector, in the order you declare them. Values are read when the game starts, so
--- changes made while playing show up on the next Play.
---
--- ```luau
--- local Field = require("@aquila/Field")
---
--- local speed = Field.number("speed", 5, { min = 0, max = 20, slider = true })
--- local target = Field.entity("target")
---
--- function M.on_update(self: Entity, dt: number)
---     if target and target:is_valid() then
---         self:set_position(self:get_position() + (target:get_world_position() - self:get_world_position()) * speed * dt)
---     end
--- end
--- ```
local Field = {}

--- Inspector limits for Field.number. `step` is the drag speed, `integer` rounds the value.
export type NumberHints = {
	min: number?,
	max: number?,
	step: number?,
	slider: boolean?,
	integer: boolean?,
}

function Field.number(name: string, default: number, hints: NumberHints?): number
	local limits: NumberHints = hints or {}
	return native.number(
		name,
		default,
		limits.min or -math.huge,
		limits.max or math.huge,
		limits.step or 0,
		limits.slider == true,
		limits.integer == true
	)
end

function Field.boolean(name: string, default: boolean): boolean
	return native.boolean(name, default)
end

function Field.string(name: string, default: string): string
	return native.string(name, default)
end

function Field.vector(name: string, default: vector): vector
	return native.vector(name, default, false)
end

--- A vector edited with a color picker, RGB from 0 to 1.
function Field.color(name: string, default: vector): vector
	return native.vector(name, default, true)
end

--- Another entity, picked by dragging it from the hierarchy onto the field. nil until one is set, or when the
--- entity it points at is gone. Saved by UUID, so it survives saving, loading and Play.
function Field.entity(name: string): Entity?
	return native.entity(name)
end

return Field
