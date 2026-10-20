#include "Aquila/Scene/ComponentRegistry.h"

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/Photometry.h"
#include "Aquila/Graphics/Material/Material.h"
#include "Aquila/Graphics/Resources/MeshLibrary.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Components/FogComponent.h"
#include "Aquila/Scene/Components/FullscreenEffectComponent.h"
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
using Reflection::PropertyMark;

namespace {

constexpr std::array<PropertyMark, 6> k_illuminance_marks = { {
	{ .value = 0.25F, .label = "Full moon" },
	{ .value = 10.F, .label = "Twilight" },
	{ .value = 400.F, .label = "Sunrise" },
	{ .value = 2000.F, .label = "Overcast day" },
	{ .value = 20000.F, .label = "Hazy sun" },
	{ .value = 100000.F, .label = "Clear noon sun" },
} };

constexpr std::array<PropertyMark, 6> k_lumen_marks = { {
	{ .value = 12.F, .label = "Candle" },
	{ .value = 450.F, .label = "40 W bulb" },
	{ .value = 800.F, .label = "60 W bulb" },
	{ .value = 1600.F, .label = "100 W bulb" },
	{ .value = 10000.F, .label = "Floodlight" },
	{ .value = 100000.F, .label = "Stadium light" },
} };

constexpr std::array<PropertyMark, 5> k_candela_marks = { {
	{ .value = 1.F, .label = "Candle" },
	{ .value = 65.F, .label = "60 W bulb" },
	{ .value = 1000.F, .label = "Flashlight" },
	{ .value = 20000.F, .label = "Car headlight" },
	{ .value = 200000.F, .label = "Searchlight" },
} };

constexpr std::array<PropertyMark, 6> k_temperature_marks = { {
	{ .value = 1900.F, .label = "Candle" },
	{ .value = 2700.F, .label = "Tungsten bulb" },
	{ .value = 3400.F, .label = "Halogen lamp" },
	{ .value = 5000.F, .label = "Horizon daylight" },
	{ .value = 6500.F, .label = "Overcast sky" },
	{ .value = 9000.F, .label = "Blue sky" },
} };

constexpr std::array<PropertyMark, 4> k_sun_size_marks = { {
	{ .value = Foundation::Photometry::SUN_ANGULAR_DIAMETER, .label = "Clear sky sun" },
	{ .value = 1.5F, .label = "Hazy sun" },
	{ .value = 5.F, .label = "Thin clouds" },
	{ .value = 15.F, .label = "Overcast glow" },
} };

constexpr std::array<PropertyMark, 6> k_exposure_marks = { {
	{ .value = -2.F, .label = "Full moon night" },
	{ .value = 3.F, .label = "Candle light" },
	{ .value = 7.F, .label = "Indoors" },
	{ .value = 10.F, .label = "Sunset" },
	{ .value = 12.F, .label = "Overcast day" },
	{ .value = 15.F, .label = "Sunny day" },
} };

constexpr std::array<PropertyMark, 4> k_fog_density_marks = { {
	{ .value = 0.002F, .label = "Light haze" },
	{ .value = 0.02F, .label = "Mist" },
	{ .value = 0.1F, .label = "Fog" },
	{ .value = 0.5F, .label = "Dense fog" },
} };

constexpr F32 k_min_temperature = 1667.F;
constexpr F32 k_max_temperature = 12000.F;

const std::array<Vec4, 16> &temperature_gradient() {
	static const std::array<Vec4, 16> colors = [] {
		std::array<Vec4, 16> out{};
		for (Usize i = 0; i < out.size(); ++i) {
			const F32 t = static_cast<F32>(i) / static_cast<F32>(out.size() - 1);
			Vec3 color = Foundation::Photometry::blackbody(Math::lerp(k_min_temperature, k_max_temperature, t));
			color /= Math::max(Math::max(color.r, color.g), Math::max(color.b, 1e-4F));
			out[i] = Vec4(Math::pow(color, Vec3(1.F / 2.2F)), 1.F);
		}
		return out;
	}();
	return colors;
}

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
		.property("Auto Exposure", &CameraComponent::auto_exposure, PropertyHints{ .toggle = true })
		.property("Compensation", &CameraComponent::exposure_compensation,
				  PropertyHints{ .min = -8.F, .max = 8.F, .speed = 0.02F, .precision = 1, .unit = "EV" })
		.visible_if([](const CameraComponent &camera) { return camera.auto_exposure; })
		.property("Exposure", &CameraComponent::ev100,
				  PropertyHints{ .min = -10.F,
								 .max = 24.F,
								 .precision = 1,
								 .unit = "EV",
								 .slider_min = -6.F,
								 .slider_max = 18.F,
								 .marks = k_exposure_marks })
		.visible_if([](const CameraComponent &camera) { return !camera.auto_exposure; })
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
	const auto in_unit = [](LightComponent::IntensityUnit unit) {
		return [unit](const LightComponent &light) {
			const bool local = light.get_type() == LightType::Point || light.get_type() == LightType::Spot;
			return local && light.get_intensity_unit() == unit;
		};
	};
	using IntensityUnit = LightComponent::IntensityUnit;

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
		.property("Unit", &LightComponent::get_intensity_unit, &LightComponent::set_intensity_unit)
		.options({ { .name = "Lumen (total light)", .value = static_cast<Int32>(IntensityUnit::Lumen) },
				   { .name = "Candela (brightness)", .value = static_cast<Int32>(IntensityUnit::Candela) } })
		.visible_if(has_range)
		.property("Intensity", &LightComponent::get_intensity, &LightComponent::set_intensity,
				  PropertyHints{ .min = 0.F,
								 .max = 1000000.F,
								 .precision = 2,
								 .unit = "lx",
								 .logarithmic = true,
								 .slider_min = 0.01F,
								 .slider_max = 150000.F,
								 .marks = k_illuminance_marks })
		.visible_if(is_directional)
		.property("Intensity", &LightComponent::get_intensity, &LightComponent::set_intensity,
				  PropertyHints{ .min = 0.F,
								 .max = 10000000.F,
								 .precision = 1,
								 .unit = "lm",
								 .logarithmic = true,
								 .slider_min = 1.F,
								 .slider_max = 1000000.F,
								 .marks = k_lumen_marks })
		.visible_if(in_unit(IntensityUnit::Lumen))
		.property("Intensity", &LightComponent::get_candela, &LightComponent::set_candela,
				  PropertyHints{ .min = 0.F,
								 .max = 10000000.F,
								 .precision = 2,
								 .unit = "cd",
								 .logarithmic = true,
								 .slider_min = 0.1F,
								 .slider_max = 1000000.F,
								 .marks = k_candela_marks })
		.visible_if(in_unit(IntensityUnit::Candela))
		.property("Use Temperature", &LightComponent::get_use_temperature, &LightComponent::set_use_temperature,
				  PropertyHints{ .toggle = true })
		.property("Temperature", &LightComponent::get_temperature, &LightComponent::set_temperature,
				  PropertyHints{ .min = k_min_temperature,
								 .max = k_max_temperature,
								 .precision = 0,
								 .unit = "K",
								 .marks = k_temperature_marks,
								 .gradient = temperature_gradient() })
		.visible_if([](const LightComponent &light) { return light.get_use_temperature(); })
		.property("Cast Shadows", &LightComponent::get_cast_shadows, &LightComponent::set_cast_shadows,
				  PropertyHints{ .toggle = true })
		.property("Range", &LightComponent::get_range, &LightComponent::set_range,
				  PropertyHints{ .min = 0.F, .max = 200.F, .speed = 0.5F, .unit = "m" })
		.visible_if(has_range)
		.property("Radius", &LightComponent::get_source_radius, &LightComponent::set_source_radius,
				  PropertyHints{ .min = 0.F, .max = 5.F, .speed = 0.005F, .precision = 3, .unit = "m" })
		.visible_if(has_range)
		.property("Sun Size", &LightComponent::get_angular_diameter, &LightComponent::set_angular_diameter,
				  PropertyHints{ .min = 0.F,
								 .max = 20.F,
								 .precision = 2,
								 .unit = "°",
								 .logarithmic = true,
								 .slider_min = 0.1F,
								 .slider_max = 20.F,
								 .marks = k_sun_size_marks })
		.visible_if(is_directional)
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

	registry.register_component<FogComponent>("Fog")
		.property("Density", &FogComponent::density,
				  PropertyHints{ .min = 0.F,
								 .max = 1.F,
								 .precision = 4,
								 .unit = "/m",
								 .logarithmic = true,
								 .slider_min = 0.0001F,
								 .slider_max = 1.F,
								 .marks = k_fog_density_marks })
		.property("Height Falloff", &FogComponent::height_falloff,
				  PropertyHints{ .min = 0.F, .max = 5.F, .speed = 0.005F, .precision = 3, .unit = "/m" })
		.property("Base Height", &FogComponent::base_height,
				  PropertyHints{ .speed = 0.05F, .precision = 2, .unit = "m" })
		.property("Color", &FogComponent::color, PropertyHints{ .color = true })
		.property("Anisotropy", &FogComponent::anisotropy,
				  PropertyHints{ .min = -0.9F, .max = 0.9F, .speed = 0.01F, .precision = 2, .slider = true })
		.property("Max Opacity", &FogComponent::max_opacity,
				  PropertyHints{ .min = 0.F, .max = 1.F, .speed = 0.01F, .precision = 2, .slider = true })
		.property("Start Distance", &FogComponent::start_distance,
				  PropertyHints{ .min = 0.F, .max = 10000.F, .speed = 0.1F, .precision = 1, .unit = "m" })
		.property("Active", &FogComponent::active, PropertyHints{ .toggle = true });

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

	registry.register_component<FullscreenEffectComponent>("Fullscreen Effect")
		.property("Material", &FullscreenEffectComponent::material, PropertyHints{ .asset_type = "material" })
		.property("Order", &FullscreenEffectComponent::order, PropertyHints{ .speed = 0.1F })
		.property("Active", &FullscreenEffectComponent::active, PropertyHints{ .toggle = true });

	registry.register_component<MaterialComponent>("Material")
		.property(
			"Material", [](const MaterialComponent &m) { return m.asset; },
			[](MaterialComponent &m, const std::string &path) {
				if (path == m.asset) {
					return;
				}
				if (path.empty() && m.material) {
					m.embedded = m.material->get_data();
				}
				m.asset = path;
				m.material = nullptr;
			},
			PropertyHints{ .asset_type = "material" });
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
