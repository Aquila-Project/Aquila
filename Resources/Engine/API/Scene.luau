--!strict

local native = require("@scene")

--- The entities of the scene the game plays. In the editor everything done here is undone on Stop.
---
--- Scripts never look entities up by name. They get them through references: an entity field set in the inspector
--- (Aquila.Field.entity), get_parent / get_children, and whatever create, clone and raycast return.
local Scene = {}

--- What Scene.raycast hit. `normal` is the face of the box it went in through, `distance` is in meters.
export type RaycastHit = {
	entity: Entity,
	position: vector,
	normal: vector,
	distance: number,
}

--- The entity with that UUID (entity:get_uuid()), nil when there's none.
function Scene.find_by_uuid(uuid: string): Entity?
	return native.find_by_uuid(uuid)
end

--- The nearest mesh the ray from `origin` along `direction` hits, nil when there's none. It tests each mesh's
--- bounding box, turned and scaled with its entity, not the exact triangles. Hidden entities, boxes the ray starts
--- inside, and `ignore` with its children are left out. `max_distance` is in meters, no limit when left out.
function Scene.raycast(origin: vector, direction: vector, max_distance: number?, ignore: Entity?): RaycastHit?
	return native.raycast(origin, direction, max_distance or math.huge, ignore)
end

--- A new entity at the origin with a transform. A taken name gets a number added, check get_name().
function Scene.create(name: string): Entity
	return native.create(name)
end

--- Destroys it and its children at the end of the frame. Their scripts' on_stop runs right away, before this
--- returns, and their on_update doesn't run again. is_valid() is false after the frame, every other method errors
--- then. Destroying a destroyed entity does nothing.
function Scene.destroy(entity: Entity)
	native.destroy(entity)
end

return Scene
