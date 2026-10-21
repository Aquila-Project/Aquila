#include "Aquila/Scene/Visibility.h"

#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Scene/Components/SceneNodeComponent.h"

namespace Aquila::SceneManagement {

bool is_visible_in_hierarchy(const entt::registry &registry, entt::entity entity) {
	constexpr int K_MAX_DEPTH = 64;
	for (int depth = 0; depth < K_MAX_DEPTH && registry.valid(entity); ++depth) {
		if (const auto *metadata = registry.try_get<Components::MetadataComponent>(entity);
			metadata != nullptr && !metadata->is_visible()) {
			return false;
		}
		const auto *node = registry.try_get<Components::SceneNodeComponent>(entity);
		if (node == nullptr || node->parent.is_null()) {
			return true;
		}
		entity = static_cast<entt::entity>(node->parent);
	}
	return true;
}

}
