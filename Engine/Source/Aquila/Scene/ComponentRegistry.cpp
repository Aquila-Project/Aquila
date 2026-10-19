#include "Aquila/Scene/ComponentRegistry.h"

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Graphics/Material/Material.h"
#include "Aquila/Graphics/Resources/MeshLibrary.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Components/LightComponent.h"
#include "Aquila/Scene/Components/MaterialComponent.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Scene/Components/SceneNodeComponent.h"
#include "Aquila/Scene/Components/SkyLightComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"

namespace Aquila::SceneManagement {

using namespace Aquila::SceneManagement::Components;
using Reflection::PropertyHints;

namespace {

void register_builtin_components(ComponentRegistry &registry) {
	registry.register_component<TransformComponent>("Transform", false, ComponentCategory::Object)
		.property("Position", &TransformComponent::get_local_position, &TransformComponent::set_local_position,
				  PropertyHints{ .speed = 0.05F, .precision = 3, .unit = "m" })
		.property("Scale", &TransformComponent::get_local_scale, &TransformComponent::set_local_scale,
				  PropertyHints{ .speed = 0.01F, .precision = 3 })
		.property("Rotation", &TransformComponent::get_local_rotation_euler,
				  &TransformComponent::set_local_rotation_euler,
				  PropertyHints{ .speed = 0.5F, .precision = 1, .unit = "°" });

	registry.register_component<MetadataComponent>("Metadata", false, ComponentCategory::Object)
		.read_only("UUID", [](const MetadataComponent &metadata) { return metadata.get_id().to_string(); })
		.property("Visible", &MetadataComponent::is_visible, &MetadataComponent::set_visible);

	registry.register_component<SceneNodeComponent>("Scene Node", false, ComponentCategory::Object)
		.read_only("Parent",
				   [](const SceneNodeComponent &node) {
					   return node.parent.is_null() || !node.parent.exists() ? std::string("None")
																			 : node.parent.get_name();
				   })
		.read_only("Children", [](const SceneNodeComponent &node) { return std::to_string(node.children.size()); });

	registry.register_component<CameraComponent>("Camera")
		.property("FOV", &CameraComponent::fov,
				  PropertyHints{ .min = 1.F, .max = 179.F, .speed = 0.5F, .precision = 1, .unit = "°" })
		.property("Near", &CameraComponent::near_plane,
				  PropertyHints{ .min = 0.001F, .max = 100.F, .speed = 0.01F, .precision = 3, .unit = "m" })
		.property("Far", &CameraComponent::far_plane,
				  PropertyHints{ .min = 0.1F, .max = 10000.F, .speed = 0.01F, .precision = 1, .unit = "m" })
		.property("Primary", &CameraComponent::primary, PropertyHints{ .toggle = true })
		.property("Orthographic", &CameraComponent::is_orthographic);

	using LightType = LightComponent::Type;
	const auto is_directional = [](const LightComponent &light) { return light.get_type() == LightType::Directional; };
	const auto has_range = [](const LightComponent &light) {
		return light.get_type() == LightType::Point || light.get_type() == LightType::Spot;
	};
	const auto has_direction = [](const LightComponent &light) {
		return light.get_type() == LightType::Directional || light.get_type() == LightType::Spot;
	};
	const auto is_spot = [](const LightComponent &light) { return light.get_type() == LightType::Spot; };

	registry.register_component<LightComponent>("Light")
		.property("Type", &LightComponent::get_type,
				  [](LightComponent &light, LightType type) {
					  if ((type == LightType::Directional) != (light.get_type() == LightType::Directional)) {
						  light.set_intensity(LightComponent::default_intensity(type));
					  }
					  light.set_type(type);
				  })
		.options({ { .name = "Directional", .value = static_cast<Int32>(LightType::Directional) },
				   { .name = "Point", .value = static_cast<Int32>(LightType::Point) },
				   { .name = "Spot", .value = static_cast<Int32>(LightType::Spot) } })
		.property("Color", &LightComponent::get_color, &LightComponent::set_color, PropertyHints{ .color = true })
		.property("Intensity", &LightComponent::get_intensity, &LightComponent::set_intensity,
				  PropertyHints{ .min = 0.F, .max = 100.F, .speed = 0.5F })
		.property("Range", &LightComponent::get_range, &LightComponent::set_range,
				  PropertyHints{ .min = 0.F, .max = 200.F, .speed = 0.5F, .unit = "m" })
		.visible_if(has_range)
		.property(
			"Direction", &LightComponent::get_direction,
			[](LightComponent &light, const Vec3 &direction) {
				if (Math::length_sq(direction) > 1e-8F) {
					light.set_direction(direction);
				}
			},
			PropertyHints{ .speed = 0.01F, .precision = 3 })
		.visible_if(has_direction)
		.property("Inner Cone", &LightComponent::get_inner_cone_angle, &LightComponent::set_inner_cone_angle,
				  PropertyHints{ .min = 0.F, .max = 89.F, .speed = 0.5F, .precision = 1, .unit = "°" })
		.visible_if(is_spot)
		.property("Outer Cone", &LightComponent::get_outer_cone_angle, &LightComponent::set_outer_cone_angle,
				  PropertyHints{ .min = 0.F, .max = 89.F, .speed = 0.5F, .precision = 1, .unit = "°" })
		.visible_if(is_spot)
		.property("Active", &LightComponent::is_active, &LightComponent::set_active, PropertyHints{ .toggle = true });

	registry.register_component<SkyLightComponent>("Sky Light")
		.property("Source", &SkyLightComponent::get_source, &SkyLightComponent::set_source)
		.options({ { .name = "Procedural", .value = static_cast<Int32>(SkySource::Procedural) },
				   { .name = "HDR Image", .value = static_cast<Int32>(SkySource::HdrImage) } })
		.property("Sun Elevation", &SkyLightComponent::get_sun_elevation, &SkyLightComponent::set_sun_elevation,
				  PropertyHints{ .min = -10.F, .max = 90.F, .speed = 0.5F, .precision = 1, .unit = "°" })
		.property("Sun Azimuth", &SkyLightComponent::get_sun_azimuth, &SkyLightComponent::set_sun_azimuth,
				  PropertyHints{ .min = 0.F, .max = 360.F, .speed = 1.F, .precision = 1, .unit = "°" })
		.property("Turbidity", &SkyLightComponent::get_turbidity, &SkyLightComponent::set_turbidity,
				  PropertyHints{ .min = 1.F, .max = 10.F, .speed = 0.05F, .precision = 2, .slider = true })
		.property("Ground Albedo", &SkyLightComponent::get_ground_albedo, &SkyLightComponent::set_ground_albedo,
				  PropertyHints{ .color = true })
		.property("Intensity", &SkyLightComponent::get_intensity, &SkyLightComponent::set_intensity,
				  PropertyHints{ .min = 0.F, .max = 20.F, .speed = 0.05F, .precision = 3 })
		.property("Tint", &SkyLightComponent::get_tint, &SkyLightComponent::set_tint, PropertyHints{ .color = true })
		.property("Active", &SkyLightComponent::is_active, &SkyLightComponent::set_active,
				  PropertyHints{ .toggle = true })
		.property(
			"Render Skybox", [](const SkyLightComponent &sky) { return sky.m_render_skybox; },
			&SkyLightComponent::set_render_skybox, PropertyHints{ .toggle = true });

	registry.register_component<MeshComponent>("Mesh").property(
		"Model",
		[](const MeshComponent &mesh) {
			using Graphics::Resources::MeshSourceKind;
			const bool is_model = mesh.data && mesh.data->get_source().kind == MeshSourceKind::Asset;
			return is_model ? mesh.data->get_source().path : std::string();
		},
		[](MeshComponent &mesh, const std::string &path) {
			if (path.empty()) {
				mesh.set_mesh(nullptr);
				return;
			}
			if (Ref<Graphics::Resources::Mesh> model =
					Graphics::Resources::MeshLibrary::instance().load(Graphics::Resources::MeshSource::asset(path))) {
				mesh.set_mesh(model);
			}
		},
		PropertyHints{ .asset_type = "model" });

	registry.register_component<MeshComponent>("Shadows", false, ComponentCategory::Rendering)
		.property("Cast Shadows", &MeshComponent::cast_shadows)
		.property("Receive Shadows", &MeshComponent::receive_shadows);

	registry.register_component<MaterialComponent>("Material")
		.property("Type", &MaterialComponent::type)
		.options({ { .name = "PBR", .value = static_cast<Int32>(Graphics::MaterialType::PBR) },
				   { .name = "Lit", .value = static_cast<Int32>(Graphics::MaterialType::Lit) },
				   { .name = "Unlit", .value = static_cast<Int32>(Graphics::MaterialType::Unlit) },
				   { .name = "Custom", .value = static_cast<Int32>(Graphics::MaterialType::Custom) } })
		.property(
			"Albedo", [](MaterialComponent &m) -> Vec4 & { return m.surface_properties.albedo; },
			PropertyHints{ .color = true })
		.property(
			"Metallic", [](MaterialComponent &m) -> F32 & { return m.surface_properties.metallic; },
			PropertyHints{ .min = 0.F, .max = 1.F, .speed = 0.01F, .precision = 3, .slider = true })
		.property(
			"Roughness", [](MaterialComponent &m) -> F32 & { return m.surface_properties.roughness; },
			PropertyHints{ .min = 0.F, .max = 1.F, .speed = 0.01F, .precision = 3, .slider = true });
}

} // namespace

ComponentRegistry &ComponentRegistry::instance() {
	static ComponentRegistry registry;
	return registry;
}

ComponentRegistry::ComponentRegistry() {
	register_builtin_components(*this);
}

const ComponentDescriptor *ComponentRegistry::find(std::string_view name) const {
	for (const auto &descriptor : m_descriptors) {
		if (descriptor->get_name() == name) {
			return descriptor.get();
		}
	}
	return nullptr;
}

void ComponentRegistry::copy_components(Entity from, Entity to) const {
	for (const auto &[type, copy] : m_copiers) {
		copy(from, to);
	}
}

void ComponentRegistry::add_copier(entt::id_type type, Delegate<void(Entity, Entity)> copy) {
	const bool known = std::ranges::any_of(m_copiers, [type](const auto &copier) { return copier.first == type; });
	if (!known) {
		m_copiers.emplace_back(type, std::move(copy));
	}
}

} // namespace Aquila::SceneManagement
