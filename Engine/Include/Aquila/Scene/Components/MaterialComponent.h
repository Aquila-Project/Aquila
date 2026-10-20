#pragma once
#include "Aquila/Foundation/Signal.h"
#include "Aquila/Graphics/Material/Material.h"
#include "Aquila/Graphics/Material/MaterialData.h"

namespace Aquila::SceneManagement::Components {

/// What an entity is drawn with: a .aqmat from the project (`asset`, shared with every entity that uses it) or, when
/// `asset` is empty, the entity's own material (`embedded`, saved with the scene). `material` is the GPU side,
/// built by ensure_material() (DefaultScenes.h): the renderer calls it, so setting `asset` or `embedded` and clearing
/// `material` is enough to switch.
struct MaterialComponent {
	std::string asset;
	Graphics::MaterialData embedded;
	Ref<Graphics::Material> material;

	/// Index into this frame's surface buffer (debug views), SceneFrameData sets it every frame.
	Uint32 material_index = UINT32_MAX;

	Signal<void()> on_changed;

	MaterialComponent() = default;
	explicit MaterialComponent(Ref<Graphics::Material> mat) : material(std::move(mat)) {
		if (material) {
			embedded = material->get_data();
		}
	}
	explicit MaterialComponent(Graphics::MaterialData data) : embedded(std::move(data)) {}

	[[nodiscard]] bool uses_asset() const { return !asset.empty(); }
};

} // namespace Aquila::SceneManagement::Components
