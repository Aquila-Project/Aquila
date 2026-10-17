#pragma once
#include "Aquila/Foundation/Signal.h"
#include "Aquila/Graphics/Material/Material.h"
#include "Aquila/Graphics/Material/MaterialDefinition.h"
#include "Aquila/Graphics/SurfaceData.h"

namespace Aquila::SceneManagement::Components {

struct MaterialComponent {
	Ref<Graphics::Material> material;

	Graphics::MaterialType type = Graphics::MaterialType::Lit;

	Graphics::GpuSurfaceData surface_properties;

	/// Index into this frame's material buffer, SceneFrameData sets it every frame.
	Uint32 material_index = UINT32_MAX;

	Signal<void()> on_changed;

	MaterialComponent() = default;
	explicit MaterialComponent(Graphics::MaterialType t) : type(t) {}
	explicit MaterialComponent(Ref<Graphics::Material> mat)
		: material(std::move(mat)), type(material ? material->get_type() : Graphics::MaterialType::Lit) {}

	void sync_type() {
		if (material) {
			type = material->get_type();
		}
	}

	static MaterialComponent from_definition(const std::string &definition_name) {
		MaterialComponent comp;
		if (const auto *def = Graphics::MaterialRegistry::get()->find(definition_name)) {
			comp.type = def->type;
			comp.surface_properties = def->defaults;
		}
		return comp;
	}
};

} // namespace Aquila::SceneManagement::Components
