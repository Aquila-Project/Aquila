#include <doctest.h>

#include "Aquila/Graphics/Resources/Mesh.h"
#include "Aquila/Graphics/Resources/ProceduralMeshes.h"
#include "Aquila/Scene/Components/LightComponent.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scene/SceneStatistics.h"

using namespace Aquila;
using namespace Aquila::SceneManagement;
using namespace Aquila::SceneManagement::Components;
using Aquila::Graphics::Resources::Mesh;

namespace {

Ref<Mesh> make_cube(const char *name) {
	auto mesh = std::make_shared<Mesh>(name);
	mesh->set_data(*Graphics::Resources::MeshGenerators::instance().generate(
		Graphics::Resources::MeshSource::procedural("cube")));
	return mesh;
}

Entity add_cube(Scene &scene, const char *name, const Ref<Mesh> &mesh) {
	Entity entity = scene.get_entity_manager()->create_entity(name);
	entity.add_component<MeshComponent>().set_mesh(mesh);
	return entity;
}

} // namespace

TEST_SUITE("Scene statistics") {
	TEST_CASE("an empty scene has no geometry") {
		Scene scene("Test");
		StatisticsCollector collector;

		const SceneStatistics statistics = collector.collect(scene, Entity::null());
		CHECK(statistics.objects == 0);
		CHECK(statistics.meshes == 0);
		CHECK(statistics.total.vertices == 0);
		CHECK(statistics.total.triangles == 0);
		CHECK(statistics.selected_objects == 0);
	}

	TEST_CASE("a cube counts eight unique vertices and twelve triangles") {
		Scene scene("Test");
		add_cube(scene, "Cube", make_cube("cube"));
		StatisticsCollector collector;

		const SceneStatistics statistics = collector.collect(scene, Entity::null());
		CHECK(statistics.objects == 1);
		CHECK(statistics.meshes == 1);
		CHECK(statistics.total.vertices == 8);
		CHECK(statistics.total.triangles == 12);
	}

	TEST_CASE("every instance of a shared mesh is counted") {
		Scene scene("Test");
		const Ref<Mesh> cube = make_cube("cube");
		add_cube(scene, "A", cube);
		add_cube(scene, "B", cube);
		scene.get_entity_manager()->create_entity("Light").add_component<LightComponent>();
		StatisticsCollector collector;

		const SceneStatistics statistics = collector.collect(scene, Entity::null());
		CHECK(statistics.objects == 3);
		CHECK(statistics.meshes == 2);
		CHECK(statistics.lights == 1);
		CHECK(statistics.total.vertices == 16);
		CHECK(statistics.total.triangles == 24);
	}

	TEST_CASE("the selection is reported separately from the total") {
		Scene scene("Test");
		const Ref<Mesh> cube = make_cube("cube");
		Entity picked = add_cube(scene, "A", cube);
		add_cube(scene, "B", cube);
		StatisticsCollector collector;

		const SceneStatistics statistics = collector.collect(scene, picked);
		CHECK(statistics.selected_objects == 1);
		CHECK(statistics.selected.vertices == 8);
		CHECK(statistics.selected.triangles == 12);
		CHECK(statistics.total.triangles == 24);
	}

	TEST_CASE("a selected entity without a mesh adds no geometry") {
		Scene scene("Test");
		add_cube(scene, "A", make_cube("cube"));
		Entity empty = scene.get_entity_manager()->create_entity("Empty");
		StatisticsCollector collector;

		const SceneStatistics statistics = collector.collect(scene, empty);
		CHECK(statistics.selected_objects == 1);
		CHECK(statistics.selected.triangles == 0);
	}

	TEST_CASE("repeated collection gives the same numbers") {
		Scene scene("Test");
		add_cube(scene, "A", make_cube("cube"));
		StatisticsCollector collector;

		const SceneStatistics first = collector.collect(scene, Entity::null());
		const SceneStatistics second = collector.collect(scene, Entity::null());
		CHECK(first.total.vertices == second.total.vertices);
		CHECK(first.total.triangles == second.total.triangles);
	}
}
