--!strict

local native = require("@camera")

--- The camera the game looks through. Every function takes the camera last, leave it out for the active one.
--- Screen positions are in the pixels Aquila.Input.get_mouse_position() uses.
local Camera = {}

--- The camera the game renders through, nil when the scene has none.
function Camera.get_active(): Entity?
	return native.get_active()
end

--- Renders through this camera from now on. It needs a Camera component.
function Camera.set_active(camera: Entity)
	native.set_active(camera)
end

--- Vertical field of view in degrees.
function Camera.get_fov(camera: Entity?): number
	return native.get_fov(camera)
end

--- Clamped to 1 to 179 degrees.
function Camera.set_fov(degrees: number, camera: Entity?)
	native.set_fov(degrees, camera)
end

--- Nothing closer than this is drawn, in meters.
function Camera.get_near(camera: Entity?): number
	return native.get_near(camera)
end

--- Kept above 0 and below the far distance.
function Camera.set_near(distance: number, camera: Entity?)
	native.set_near(distance, camera)
end

--- Nothing further than this is drawn, in meters.
function Camera.get_far(camera: Entity?): number
	return native.get_far(camera)
end

--- Kept beyond the near distance.
function Camera.set_far(distance: number, camera: Entity?)
	native.set_far(distance, camera)
end

--- Where a world point shows up on screen (z is 0). nil when it's behind the camera or the game isn't on screen.
--- Uses the camera as of the last frame.
function Camera.world_to_screen(point: vector, camera: Entity?): vector?
	return native.world_to_screen(point, camera)
end

--- The ray under a screen point, a mouse position for instance: where it starts and the direction it goes, 1 long.
---
--- ```luau
--- local origin, direction = Camera.screen_to_ray(Input.get_mouse_position())
--- local hit = origin + direction * 10
--- ```
function Camera.screen_to_ray(point: vector, camera: Entity?): (vector, vector)
	return native.screen_ray_origin(point, camera), native.screen_ray_direction(point, camera)
end

return Camera
