#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "Aquila/Scene/ComponentRegistry.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Components/LightComponent.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Scene/Scene.h"

using namespace Aquila;
using namespace Aquila::SceneManagement;
using namespace Aquila::SceneManagement::Components;

TEST_SUITE("ComponentRegistry") {
	TEST_CASE("built-in components are registered by name") {
		const auto &registry = ComponentRegistry::instance();
		for (const char *name : { "Transform", "Camera", "Light", "Sky Light", "Mesh", "Material" }) {
			CHECK_MESSAGE(registry.find(name) != nullptr, name);
		}
		CHECK(registry.find("Nope") == nullptr);
	}

	TEST_CASE("descriptors find components on entities") {
		Scene scene("Test");
		Entity entity = scene.get_entity_manager()->create_entity("Thing");
		const auto &registry = ComponentRegistry::instance();

		const ComponentDescriptor &camera = *registry.find("Camera");
		CHECK_FALSE(camera.has(entity));
		CHECK(camera.get(entity) == nullptr);

		entity.add_component<CameraComponent>();
		CHECK(camera.has(entity));
		CHECK(camera.get(entity) == &entity.get_component<CameraComponent>());
	}

	TEST_CASE("properties read and write the real component") {
		Scene scene("Test");
		Entity entity = scene.get_entity_manager()->create_entity("Thing");
		auto &camera_component = entity.add_component<CameraComponent>();

		const ComponentDescriptor &camera = *ComponentRegistry::instance().find("Camera");
		const Reflection::Property &fov = *camera.get_type().find("FOV");
		void *instance = camera.get(entity);

		camera_component.fov = 42.F;
		CHECK(std::get<F32>(fov.get(instance)) == doctest::Approx(42.F));

		fov.set(instance, Reflection::PropertyValue{ 70.F });
		CHECK(camera_component.fov == doctest::Approx(70.F));
	}

	TEST_CASE("getter/setter properties go through the component's own logic") {
		Scene scene("Test");
		Entity entity = scene.get_entity_manager()->create_entity("Thing");
		auto &transform = entity.get_component<TransformComponent>();

		const ComponentDescriptor &descriptor = *ComponentRegistry::instance().find("Transform");
		const Reflection::Property &position = *descriptor.get_type().find("Position");
		position.set(descriptor.get(entity), Reflection::PropertyValue{ Vec3(1.F, 2.F, 3.F) });

		CHECK(transform.get_local_position().y == doctest::Approx(2.F));
		CHECK(transform.is_world_matrix_dirty());
	}

	TEST_CASE("change signals are exposed only for components that have one") {
		Scene scene("Test");
		Entity entity = scene.get_entity_manager()->create_entity("Thing");
		entity.add_component<CameraComponent>();
		entity.add_component<MeshComponent>();
		const auto &registry = ComponentRegistry::instance();

		Signal<void()> *signal = registry.find("Camera")->get_changed_signal(entity);
		REQUIRE(signal != nullptr);
		CHECK(signal == &entity.get_component<CameraComponent>().on_changed);
		CHECK(registry.find("Mesh")->get_changed_signal(entity) == nullptr);
		CHECK(registry.find("Light")->get_changed_signal(entity) == nullptr);
	}

	TEST_CASE("light properties follow the light type") {
		Scene scene("Test");
		Entity entity = scene.get_entity_manager()->create_entity("Thing");
		auto &light = entity.add_component<LightComponent>();

		const ComponentDescriptor &descriptor = *ComponentRegistry::instance().find("Light");
		const Reflection::Property &range = *descriptor.get_type().find("Range");
		const Reflection::Property &direction = *descriptor.get_type().find("Direction");
		const Reflection::Property &cone = *descriptor.get_type().find("Outer Cone");
		void *instance = descriptor.get(entity);

		light.set_type(LightComponent::Type::Directional);
		CHECK_FALSE(range.is_visible(instance));
		CHECK(direction.is_visible(instance));
		CHECK_FALSE(cone.is_visible(instance));

		light.set_type(LightComponent::Type::Point);
		CHECK(range.is_visible(instance));
		CHECK_FALSE(direction.is_visible(instance));
		CHECK_FALSE(cone.is_visible(instance));

		light.set_type(LightComponent::Type::Spot);
		CHECK(range.is_visible(instance));
		CHECK(direction.is_visible(instance));
		CHECK(cone.is_visible(instance));
	}

	TEST_CASE("the light type is editable through the registry") {
		Scene scene("Test");
		Entity entity = scene.get_entity_manager()->create_entity("Thing");
		auto &light = entity.add_component<LightComponent>();

		const ComponentDescriptor &descriptor = *ComponentRegistry::instance().find("Light");
		const Reflection::Property &type = *descriptor.get_type().find("Type");
		CHECK(type.kind == Reflection::PropertyKind::Enum);
		CHECK(type.options.size() == 3);

		type.set(descriptor.get(entity), Reflection::PropertyValue{ static_cast<Int32>(LightComponent::Type::Point) });
		CHECK(light.get_type() == LightComponent::Type::Point);
	}

	TEST_CASE("a zero light direction is ignored") {
		Scene scene("Test");
		Entity entity = scene.get_entity_manager()->create_entity("Thing");
		auto &light = entity.add_component<LightComponent>();
		light.set_direction(Vec3(0.F, -1.F, 0.F));

		const ComponentDescriptor &descriptor = *ComponentRegistry::instance().find("Light");
		descriptor.get_type().find("Direction")->set(descriptor.get(entity), Reflection::PropertyValue{ Vec3(0.F) });
		CHECK(light.get_direction() == Vec3(0.F, -1.F, 0.F));
	}

	TEST_CASE("components can be removed through their descriptor, except the transform") {
		Scene scene("Test");
		Entity entity = scene.get_entity_manager()->create_entity("Thing");
		entity.add_component<CameraComponent>();
		const auto &registry = ComponentRegistry::instance();

		const ComponentDescriptor &camera = *registry.find("Camera");
		REQUIRE(camera.is_removable());
		camera.remove(entity);
		CHECK_FALSE(camera.has(entity));

		CHECK_FALSE(registry.find("Transform")->is_removable());
	}

	TEST_CASE("each descriptor says which part of an entity it belongs to") {
		const auto &registry = ComponentRegistry::instance();
		for (const char *name : { "Transform", "Metadata", "Scene Node" }) {
			CHECK_MESSAGE(registry.find(name)->get_category() == ComponentCategory::Object, name);
			CHECK_FALSE(registry.find(name)->is_removable());
		}
		for (const char *name : { "Mesh", "Material", "Light", "Sky Light", "Camera" }) {
			CHECK_MESSAGE(registry.find(name)->get_category() == ComponentCategory::General, name);
		}
		CHECK(registry.find("Shadows")->get_category() == ComponentCategory::Rendering);
	}

	TEST_CASE("the shadows view edits the mesh's own flags") {
		Scene scene("Test");
		Entity entity = scene.get_entity_manager()->create_entity("Thing");
		const ComponentDescriptor &shadows = *ComponentRegistry::instance().find("Shadows");
		CHECK_FALSE(shadows.has(entity));

		auto &mesh = entity.add_component<MeshComponent>();
		shadows.get_type().find("Cast Shadows")->set(shadows.get(entity), Reflection::PropertyValue{ false });
		CHECK_FALSE(mesh.cast_shadows);
	}

	TEST_CASE("metadata exposes the uuid read-only and visibility editable") {
		Scene scene("Test");
		Entity entity = scene.get_entity_manager()->create_entity("Thing");
		const ComponentDescriptor &metadata = *ComponentRegistry::instance().find("Metadata");
		REQUIRE(metadata.has(entity));

		const Reflection::Property &uuid = *metadata.get_type().find("UUID");
		CHECK(uuid.hints.read_only);
		CHECK(std::get<std::string>(uuid.get(metadata.get(entity))) == entity.get_uuid().to_string());

		metadata.get_type().find("Visible")->set(metadata.get(entity), Reflection::PropertyValue{ false });
		CHECK_FALSE(entity.get_component<MetadataComponent>().is_visible());
	}

	TEST_CASE("transform values carry blender-style units") {
		const Reflection::TypeInfo &transform = ComponentRegistry::instance().find("Transform")->get_type();
		CHECK(transform.find("Position")->hints.unit == "m");
		CHECK(transform.find("Rotation")->hints.unit == "°");
		CHECK(transform.find("Scale")->hints.unit.empty());
	}
}
