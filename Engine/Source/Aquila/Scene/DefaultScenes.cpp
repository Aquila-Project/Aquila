#include "Aquila/Scene/DefaultScenes.h"

#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Graphics/Material/MaterialAssets.h"
#include "Aquila/Graphics/Resources/MeshLibrary.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Components/FogComponent.h"
#include "Aquila/Scene/Components/LightComponent.h"
#include "Aquila/Scene/Components/MaterialComponent.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/SkyLightComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/EntityManager.h"

namespace Aquila::SceneManagement {

using namespace Aquila::SceneManagement::Components;

Ref<Graphics::Material> make_default_material(GFX::GfxContext &ctx) {
	return Graphics::MaterialAssets::get()->create(ctx, Graphics::MaterialData{});
}

void ensure_material(MaterialComponent &component, GFX::GfxContext &ctx) {
	if (component.uses_asset()) {
		Ref<Graphics::Material> shared = Graphics::MaterialAssets::get()->load(ctx, component.asset);
		if (shared != component.material) {
			component.material = std::move(shared);
		}
		return;
	}
	if (!component.material || !component.material->get_asset_path().empty()) {
		component.material = Graphics::MaterialAssets::get()->create(ctx, component.embedded);
	}
}

void create_missing_materials(Scene &scene, GFX::GfxContext &ctx) {
	auto view = scene.get_registry().view<MaterialComponent>();
	for (auto entity : view) {
		ensure_material(view.get<MaterialComponent>(entity), ctx);
	}
}

Entity spawn_default_camera(Scene &scene, F32 aspect_ratio) {
	auto *em = scene.get_entity_manager();
	auto cam = em->create_entity("Camera");
	auto &cam_comp = cam.add_component<CameraComponent>();
	cam_comp.fov = 60.F;
	cam_comp.near_plane = 0.1F;
	cam_comp.far_plane = 500.F;
	cam_comp.aspect_ratio = aspect_ratio;
	cam_comp.primary = true;
	cam.get_component<TransformComponent>().set_local_position({ 0.F, 1.5F, -5.F });
	scene.set_active_camera(cam);
	return cam;
}

void populate_demo_scene(Scene &scene, GFX::GfxContext &ctx, F32 aspect_ratio) {
	auto *em = scene.get_entity_manager();
	spawn_default_camera(scene, aspect_ratio);

	const Ref<Graphics::Resources::Mesh> cube =
		Graphics::Resources::MeshLibrary::instance().load(Graphics::Resources::MeshSource::procedural("cube"));

	auto add_cube = [&](const char *name, Vec3 pos) {
		auto entity = em->create_entity(name);
		entity.add_component<MeshComponent>().set_mesh(cube);
		entity.get_component<TransformComponent>().set_local_position(pos);
		Graphics::MaterialData data;
		data.values["baseColor"] = Vec4(0.8F, 0.6F, 0.4F, 1.F);
		data.values["roughness"] = 0.6F;
		auto &mat = entity.add_component<MaterialComponent>(std::move(data));
		ensure_material(mat, ctx);
		return entity;
	};
	add_cube("CubeA", { -2.5F, 1.F, 2.F });
	add_cube("CubeB", { 2.5F, 1.F, 2.F });
	auto floor = add_cube("Floor", { 0.0F, 0.F, 2.F });
	floor.get_component<TransformComponent>().set_local_scale({ 12.F, 0.2F, 12.F });

	{
		auto e = em->create_entity("SunLight");
		auto &light = e.add_component<LightComponent>(LightComponent::Type::Directional, Vec3(1.0F, 0.95F, 0.8F),
													  LightComponent::DEFAULT_ILLUMINANCE);
		light.set_direction(-SkyLightComponent{}.get_sun_direction());
	}
	{
		auto e = em->create_entity("PointA");
		e.get_component<TransformComponent>().set_local_position({ -1.5F, 0.5F, 1.5F });
		auto &light = e.add_component<LightComponent>(LightComponent::Type::Point, Vec3(1.0F, 0.4F, 0.1F),
													  LightComponent::DEFAULT_POWER);
		light.set_range(6.0F);
	}
	{
		auto e = em->create_entity("PointB");
		e.get_component<TransformComponent>().set_local_position({ 1.5F, 0.5F, 1.5F });
		auto &light = e.add_component<LightComponent>(LightComponent::Type::Point, Vec3(0.2F, 0.5F, 1.0F),
													  LightComponent::DEFAULT_POWER);
		light.set_range(6.0F);
	}
	{
		auto e = em->create_entity("Sky");
		e.add_component<SkyLightComponent>();
	}
	{
		auto e = em->create_entity("Fog");
		e.add_component<FogComponent>();
	}
}

} // namespace Aquila::SceneManagement
