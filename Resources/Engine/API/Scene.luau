--!strict

local native = require("@scene")

--- The entities of the scene the game plays. In the editor everything done here is undone on Stop.
local Scene = {}

--- The first entity with that name, nil when there's none.
function Scene.find(name: string): Entity?
	return native.find(name)
end

--- The entity with that UUID (entity:get_uuid()), nil when there's none.
function Scene.find_by_uuid(uuid: string): Entity?
	return native.find_by_uuid(uuid)
end

--- A new entity at the origin with a transform. A taken name gets a number added, check get_name().
function Scene.create(name: string): Entity
	return native.create(name)
end

--- Destroys it and its children at the end of the frame. is_valid() is false after that, every other method
--- errors. Destroying a destroyed entity does nothing.
function Scene.destroy(entity: Entity)
	native.destroy(entity)
end

return Scene
