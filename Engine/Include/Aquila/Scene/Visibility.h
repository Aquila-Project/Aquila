#ifndef AQUILA_SCENE_VISIBILITY_H
#define AQUILA_SCENE_VISIBILITY_H

#include <entt.h>

namespace Aquila::SceneManagement {

/// False when the entity or any of its parents is hidden.
[[nodiscard]] bool is_visible_in_hierarchy(const entt::registry &registry, entt::entity entity);

}

#endif
