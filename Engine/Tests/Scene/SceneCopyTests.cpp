#include <doctest.h>

#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Scene/Components/OutlineComponent.h"
#include "Aquila/Scene/Components/SceneNodeComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Scene/Scene.h"

using namespace Aquila;
using namespace Aquila::SceneManagement;
using namespace Aquila::SceneManagement::Components;

TEST_SUITE("Scene copy") {
	TEST_CASE("the copy keeps entity ids, components and the hierarchy") {
		Scene original("Original");
		EntityManager &entities = *original.get_entity_manager();
		Entity parent = entities.create_entity("Parent");
		Entity child = entities.create_entity("Child");
		entities.add_child(parent, child);
		parent.get_component<TransformComponent>().set_local_position(Vec3(1.F, 2.F, 3.F));
		child.get_component<TransformComponent>().set_local_position(Vec3(0.F, 1.F, 0.F));

		Scene copy;
		copy.copy_from(original);

		Entity copied_parent(parent.get_handle(), &copy);
		Entity copied_child(child.get_handle(), &copy);
		REQUIRE(copied_parent.exists());
		REQUIRE(copied_child.exists());
		CHECK(copy.get_scene_name() == "Original");
		CHECK(copied_parent.get_name() == "Parent");
		CHECK(copied_parent.get_component<MetadataComponent>().get_id() ==
			  parent.get_component<MetadataComponent>().get_id());

		const auto &node = copied_child.get_component<SceneNodeComponent>();
		CHECK(node.ent == copied_child);
		CHECK(node.parent == copied_parent);
		REQUIRE(copy.get_entity_manager()->get_children(copied_parent).size() == 1);
		CHECK(copy.get_entity_manager()->get_children(copied_parent).front() == copied_child);

		copy.update_transform_hierarchy();
		CHECK(copied_child.get_component<TransformComponent>().get_world_position() == Vec3(1.F, 3.F, 3.F));
	}

	TEST_CASE("editing the copy leaves the original alone") {
		Scene original("Original");
		Entity entity = original.get_entity_manager()->create_entity("Thing");

		Scene copy;
		copy.copy_from(original);
		Entity copied(entity.get_handle(), &copy);
		copied.get_component<TransformComponent>().set_local_position(Vec3(5.F));
		copy.get_entity_manager()->create_entity("Extra");

		CHECK(entity.get_component<TransformComponent>().get_local_position() == Vec3(0.F));
		CHECK(original.get_entity_manager()->count<MetadataComponent>() == 1);
	}

	TEST_CASE("copying a snapshot back undoes what happened since, handles stay valid") {
		Scene scene("Live");
		EntityManager &entities = *scene.get_entity_manager();
		Entity parent = entities.create_entity("Parent");
		Entity child = entities.create_entity("Child");
		entities.add_child(parent, child);

		Scene snapshot;
		snapshot.copy_from(scene);

		entities.destroy_entity(parent);
		Entity spawned = entities.create_entity("Spawned");
		REQUIRE_FALSE(parent.exists());

		scene.copy_from(snapshot);
		entities.flush_deletion_queue();

		CHECK(parent.exists());
		CHECK(child.exists());
		CHECK_FALSE(spawned.exists());
		CHECK(entities.get_children(parent).size() == 1);

		parent.get_component<TransformComponent>().set_local_position(Vec3(2.F, 0.F, 0.F));
		scene.update_transform_hierarchy();
		CHECK(child.get_component<TransformComponent>().get_world_position() == Vec3(2.F, 0.F, 0.F));
	}

	TEST_CASE("components the registry doesn't know are left out") {
		Scene original("Original");
		Entity entity = original.get_entity_manager()->create_entity("Selected");
		entity.add_component<OutlineComponent>();

		Scene copy;
		copy.copy_from(original);

		CHECK_FALSE(Entity(entity.get_handle(), &copy).has_component<OutlineComponent>());
	}
}
