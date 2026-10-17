#ifndef SCENE_NODE_COMPONENT_H
#define SCENE_NODE_COMPONENT_H

#include "Aquila/Scene/Entity.h"
namespace Aquila::SceneManagement::Components {
/// Parent and children links. `ent` is the entity that owns this component.
/// Change them through EntityManager, not by hand.
struct SceneNodeComponent {
	Entity ent;
	Entity parent;
	std::vector<Entity> children;

	SceneNodeComponent() = default;

	SceneNodeComponent(const SceneNodeComponent &other) = default;
	SceneNodeComponent &operator=(const SceneNodeComponent &other) = default;

	SceneNodeComponent(SceneNodeComponent &&other) noexcept = default;
	SceneNodeComponent &operator=(SceneNodeComponent &&other) noexcept = default;

	~SceneNodeComponent() = default;
};
} // namespace Aquila::SceneManagement::Components
#endif
