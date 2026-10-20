#include "UI/ComponentIcons.h"

#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Components/FogComponent.h"
#include "Aquila/Scene/Components/LightComponent.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/SkyLightComponent.h"

namespace Editor {

using namespace Aquila::SceneManagement::Components;

namespace {

constexpr std::array<std::pair<std::string_view, std::string_view>, 11> k_component_icons = { {
	{ "Transform", "move-3d" },
	{ "Metadata", "info" },
	{ "Scene Node", "folder-tree" },
	{ "Shadows", "moon" },
	{ "Mesh", "cuboid" },
	{ "Material", "eclipse" },
	{ "Light", "lightbulb" },
	{ "Sky Light", "sun" },
	{ "Fog", "cloud-fog" },
	{ "Fullscreen Effect", "sparkles" },
	{ "Camera", "video" },
} };

}

std::string_view component_icon(std::string_view component) {
	for (const auto &[name, icon] : k_component_icons) {
		if (name == component) {
			return icon;
		}
	}
	return "box";
}

std::string_view entity_icon(Aquila::SceneManagement::Entity entity) {
	if (entity.has_component<CameraComponent>()) {
		return component_icon("Camera");
	}
	if (entity.has_component<SkyLightComponent>()) {
		return component_icon("Sky Light");
	}
	if (entity.has_component<FogComponent>()) {
		return component_icon("Fog");
	}
	if (entity.has_component<LightComponent>()) {
		return component_icon("Light");
	}
	if (entity.has_component<MeshComponent>()) {
		return component_icon("Mesh");
	}
	return k_empty_entity_icon;
}

} // namespace Editor
