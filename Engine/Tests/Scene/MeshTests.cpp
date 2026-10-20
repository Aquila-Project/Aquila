#include <doctest.h>

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Graphics/Resources/Mesh.h"
#include "Aquila/Graphics/Resources/MeshImporter.h"
#include "Aquila/Graphics/Resources/MeshLibrary.h"
#include "Aquila/Graphics/Resources/ProceduralMeshes.h"
#include "Aquila/Platform/Filesystem/NativeFileSystem.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Components/LightComponent.h"
#include "Aquila/Scene/Components/MaterialComponent.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Scene/Components/ScriptComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Scene/Scene.h"

using namespace Aquila;
using namespace Aquila::Graphics::Resources;
using namespace Aquila::SceneManagement;
using namespace Aquila::SceneManagement::Components;

namespace {

const std::string &scene_dir() {
	static const std::string dir = [] {
		auto path = std::filesystem::temp_directory_path() / "aquila-mesh-tests";
		std::filesystem::create_directories(path);
		Platform::Filesystem::VirtualFileSystem::init();
		Platform::Filesystem::VirtualFileSystem::get()->mount(
			"/mesh-tests", std::make_shared<Platform::Filesystem::NativeFileSystem>(path.string()));
		return std::string("/mesh-tests");
	}();
	return dir;
}

Entity find(Scene &scene, std::string_view name) {
	for (auto handle : scene.get_registry().view<MetadataComponent>()) {
		Entity entity(handle, &scene);
		if (entity.get_component<MetadataComponent>().get_name() == name) {
			return entity;
		}
	}
	return {};
}

Usize count_bad_uv_triangles(const MeshData &data) {
	Usize bad = 0;
	for (Usize i = 0; i + 2 < data.indices.size(); i += 3) {
		const std::array<const RHI::Vertex *, 3> corners = { &data.vertices[data.indices[i]],
															 &data.vertices[data.indices[i + 1]],
															 &data.vertices[data.indices[i + 2]] };
		const Vec3 e1 = corners[1]->pos - corners[0]->pos;
		const Vec3 e2 = corners[2]->pos - corners[0]->pos;
		const Vec2 d1 = corners[1]->texcoord - corners[0]->texcoord;
		const Vec2 d2 = corners[2]->texcoord - corners[0]->texcoord;
		const F32 det = d1.x * d2.y - d2.x * d1.y;
		const Vec3 face_normal = Math::cross(e1, e2);
		if (Math::abs(det) < 1e-10F || Math::length(face_normal) < 1e-10F) {
			++bad;
			continue;
		}
		const Vec3 along_u = (e1 * d2.y - e2 * d1.y) / det;
		const Vec3 image_up = (e1 * d2.x - e2 * d1.x) / det;
		const bool side = Math::abs(Math::normalize(face_normal).y) < 0.5F;

		bool ok = Math::dot(Math::cross(face_normal, image_up), along_u) > 0.F && (!side || image_up.y > 0.F);
		for (const RHI::Vertex *vertex : corners) {
			const Vec3 tangent = Vec3(vertex->tangent);
			ok = ok && Math::dot(vertex->normals, face_normal) > 0.F && Math::abs(vertex->tangent.w) == 1.F &&
				Math::abs(Math::length(tangent) - 1.F) < 1e-4F && Math::dot(tangent, along_u) > 0.F &&
				Math::dot(Math::cross(vertex->normals, tangent) * vertex->tangent.w, image_up) > 0.F;
		}
		bad += ok ? 0 : 1;
	}
	return bad;
}

void write_test_file(const std::string &name, std::string_view contents) {
	scene_dir();
	std::ofstream(std::filesystem::temp_directory_path() / "aquila-mesh-tests" / name) << contents;
}

void check_front_quad(const Option<MeshData> &data) {
	REQUIRE(data.has_value());
	REQUIRE(data->indices.size() == 6);
	CHECK(count_bad_uv_triangles(*data) == 0);
	for (const RHI::Vertex &vertex : data->vertices) {
		if (vertex.texcoord.x < 0.5F && vertex.texcoord.y < 0.5F) {
			CHECK(vertex.pos.x > 0.F);
			CHECK(vertex.pos.y > 0.F);
		}
	}
}

}

TEST_SUITE("Procedural meshes") {
	TEST_CASE("every built-in generator builds a mesh from its defaults") {
		for (const char *id : { "cube", "sphere", "cylinder", "plane" }) {
			CAPTURE(id);
			const Option<MeshData> data = MeshGenerators::instance().generate(MeshSource::procedural(id));
			REQUIRE(data.has_value());
			CHECK_FALSE(data->vertices.empty());
			CHECK(data->indices.size() % 3 == 0);
			for (Uint32 index : data->indices) {
				CHECK(index < data->vertices.size());
			}
		}
		CHECK_FALSE(MeshGenerators::instance().generate(MeshSource::procedural("teapot")).has_value());
	}

	TEST_CASE("resolve fills defaults, clamps and drops unknown params") {
		const Option<MeshParams> params =
			MeshGenerators::instance().resolve("sphere", { { "segments", 1.F }, { "rings", 7.6F }, { "bogus", 3.F } });
		REQUIRE(params.has_value());
		CHECK(params->at("radius") == doctest::Approx(0.5F));
		CHECK(params->at("segments") == doctest::Approx(3.F));
		CHECK(params->at("rings") == doctest::Approx(8.F));
		CHECK_FALSE(params->contains("bogus"));
	}

	TEST_CASE("textures read left to right, stay upright and match the tangents") {
		for (const std::string id : { "cube", "sphere", "cylinder", "plane" }) {
			CAPTURE(id);
			const Option<MeshData> data = MeshGenerators::instance().generate(MeshSource::procedural(id));
			REQUIRE(data.has_value());
			CHECK(count_bad_uv_triangles(*data) == 0);
		}
	}

	TEST_CASE("the cube size is the edge length") {
		const Option<MeshData> data =
			MeshGenerators::instance().generate(MeshSource::procedural("cube", { { "size", 2.F } }));
		REQUIRE(data.has_value());
		F32 max_x = 0.F;
		for (const auto &vertex : data->vertices) {
			max_x = std::max(max_x, vertex.pos.x);
		}
		CHECK(max_x == doctest::Approx(1.F));
	}

	TEST_CASE("a mesh measures its surface, scaled per axis") {
		Option<MeshData> cube_data = MeshGenerators::instance().generate(MeshSource::procedural("cube"));
		REQUIRE(cube_data.has_value());
		Mesh cube("cube");
		cube.set_data(std::move(*cube_data));
		const MeshSurface &box = cube.get_surface();
		CHECK(box.get_area() == doctest::Approx(6.F));
		CHECK(box.axis_area.x == doctest::Approx(2.F));
		CHECK(box.axis_area.y == doctest::Approx(2.F));
		CHECK(box.axis_area.z == doctest::Approx(2.F));
		CHECK(box.get_scaled_area(Vec3(2.F, 1.F, 1.F)) == doctest::Approx(10.F));
		CHECK(Math::length(box.center) < 1e-5F);

		Option<MeshData> sphere_data = MeshGenerators::instance().generate(MeshSource::procedural("sphere"));
		REQUIRE(sphere_data.has_value());
		Mesh sphere("sphere");
		sphere.set_data(std::move(*sphere_data));
		const MeshSurface &ball = sphere.get_surface();
		CHECK(ball.get_area() == doctest::Approx(Math::PI).epsilon(0.03));
		CHECK(ball.get_scaled_area(Vec3(2.F)) == doctest::Approx(4.F * ball.get_area()));
	}

	TEST_CASE("the reflected params edit a MeshParams") {
		const Reflection::TypeInfo *type = MeshGenerators::instance().get_type("sphere");
		REQUIRE(type != nullptr);
		MeshParams params = *MeshGenerators::instance().resolve("sphere", {});
		const Reflection::Property *segments = type->find("Segments");
		REQUIRE(segments != nullptr);
		CHECK(segments->kind == Reflection::PropertyKind::Int);
		CHECK(std::get<Int32>(segments->get(&params)) == 32);
		segments->set(&params, Reflection::PropertyValue{ 1000 });
		CHECK(params.at("segments") == doctest::Approx(256.F));
	}

	TEST_CASE("old procedural paths still load") {
		const Option<MeshSource> sphere = MeshSource::from_legacy_path("procedural://sphere/0.25/12/6");
		REQUIRE(sphere.has_value());
		CHECK(sphere->generator == "sphere");
		CHECK(sphere->params.at("radius") == doctest::Approx(0.25F));
		CHECK(sphere->params.at("segments") == doctest::Approx(12.F));
		CHECK(sphere->params.at("rings") == doctest::Approx(6.F));

		CHECK(MeshSource::from_legacy_path("procedural://cube/0.5")->params.at("size") == doctest::Approx(1.F));
		CHECK(MeshSource::from_legacy_path("procedural://cube")->params.at("size") == doctest::Approx(2.F));
		CHECK_FALSE(MeshSource::from_legacy_path("procedural://teapot").has_value());
		CHECK_FALSE(MeshSource::from_legacy_path("").has_value());

		const Option<MeshSource> model = MeshSource::from_legacy_path("/project/Content/statue.glb");
		REQUIRE(model.has_value());
		CHECK(model->kind == MeshSourceKind::Asset);
		CHECK(model->path == "/project/Content/statue.glb");
	}

	TEST_CASE("changing a mesh bumps its revision") {
		Mesh mesh("Test");
		const Uint64 before = mesh.get_revision();
		mesh.set_data(*MeshGenerators::instance().generate(MeshSource::procedural("cube")));
		CHECK(mesh.get_revision() > before);
		CHECK(mesh.get_primitives().size() == 1);
		CHECK(mesh.get_source().kind == MeshSourceKind::None);
	}
}

TEST_SUITE("Mesh importer") {
	TEST_CASE("an OBJ quad facing +Z comes in upright and not mirrored") {
		write_test_file("quad.obj",
						"v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
						"vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
						"vn 0 0 1\n"
						"f 1/1/1 2/2/1 3/3/1 4/4/1\n");
		check_front_quad(import_mesh(scene_dir() + "/quad.obj"));
	}

	TEST_CASE("a glTF quad with its own tangents comes in upright and not mirrored") {
		write_test_file(
			"quad.gltf",
			R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],)"
			R"("meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TANGENT":2,"TEXCOORD_0":3},"indices":4}]}],)"
			R"("buffers":[{"byteLength":204,"uri":"data:application/octet-stream;base64,)"
			R"(AACAvwAAgL8AAAAAAACAPwAAgL8AAAAAAACAPwAAgD8AAAAAAACAvwAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAA)"
			R"(AAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AACAPwAAAAAAAAAAAACAPwAAgD8AAAAAAAAAAAAAgD8AAIA/)"
			R"(AAAAAAAAAAAAAIA/AACAPwAAAAAAAAAAAACAPwAAAAAAAIA/AACAPwAAgD8AAIA/AAAAAAAAAAAAAAAAAAABAAIA)"
			R"(AAACAAMA)"
			R"("}],"bufferViews":[{"buffer":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":48},)"
			R"({"buffer":0,"byteOffset":96,"byteLength":64},{"buffer":0,"byteOffset":160,"byteLength":32},)"
			R"({"buffer":0,"byteOffset":192,"byteLength":12}],"accessors":[)"
			R"({"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[-1,-1,0],"max":[1,1,0]},)"
			R"({"bufferView":1,"componentType":5126,"count":4,"type":"VEC3"},)"
			R"({"bufferView":2,"componentType":5126,"count":4,"type":"VEC4"},)"
			R"({"bufferView":3,"componentType":5126,"count":4,"type":"VEC2"},)"
			R"({"bufferView":4,"componentType":5123,"count":6,"type":"SCALAR"}]})");
		check_front_quad(import_mesh(scene_dir() + "/quad.gltf"));
	}
}

TEST_SUITE("Mesh library") {
	TEST_CASE("equal sources share one mesh, defaults included") {
		MeshLibrary &library = MeshLibrary::instance();
		const Ref<Mesh> a = library.load(MeshSource::procedural("sphere"));
		const Ref<Mesh> b = library.load(MeshSource::procedural("sphere", { { "radius", 0.5F } }));
		const Ref<Mesh> c = library.load(MeshSource::procedural("sphere", { { "radius", 1.F } }));
		REQUIRE(a != nullptr);
		CHECK(a == b);
		CHECK(a != c);
		CHECK(a->get_source().params.size() == 3);
	}

	TEST_CASE("a mesh dies with its last user") {
		MeshLibrary &library = MeshLibrary::instance();
		std::weak_ptr<Mesh> weak = library.load(MeshSource::procedural("cylinder", { { "height", 3.F } }));
		CHECK(weak.expired());
	}

	TEST_CASE("nothing to load gives null") {
		MeshLibrary &library = MeshLibrary::instance();
		CHECK(library.load(MeshSource{}) == nullptr);
		CHECK(library.load(MeshSource::procedural("teapot")) == nullptr);
		scene_dir();
		CHECK(library.load(MeshSource::asset("/mesh-tests/missing.glb")) == nullptr);
	}
}

TEST_SUITE("Scene serialization") {
	TEST_CASE("rotations survive a save and a load, old files with only x, y, z load right") {
		auto same_rotation = [](const Quaternion &a, const Quaternion &b) {
			return std::abs(std::abs(glm::dot(a, b)) - 1.F) < 1e-5F;
		};
		const Quaternion turned = Quaternion(Math::radians(Vec3(30.F, 135.F, -20.F)));
		const std::string path = scene_dir() + "/rotation.aqscene";
		{
			Scene scene("Saved");
			Entity thing = scene.get_entity_manager()->create_entity("Thing");
			thing.get_component<TransformComponent>().set_local_rotation(turned);
			REQUIRE(scene.serialize(path));
		}
		Scene loaded("Loaded");
		REQUIRE(loaded.deserialize(path));
		CHECK(same_rotation(find(loaded, "Thing").get_component<TransformComponent>().get_local_rotation(), turned));

		const Quaternion legacy = Quaternion(Math::radians(Vec3(0.F, 120.F, 0.F)));
		const std::string old_path = scene_dir() + "/old_rotation.aqscene";
		std::ofstream(std::filesystem::temp_directory_path() / "aquila-mesh-tests" / "old_rotation.aqscene")
			<< "{ \"scene_name\": \"Old\", \"entities\": { \"0\": { \"components\": {"
			   " \"metadata_component\": { \"name\": \"Thing\", \"uuid\": \""
			<< Foundation::UUID::generate().to_string()
			<< "\" }, \"transform_component\": { \"position\": [0, 0, 0], \"rotation\": [" << legacy.x << ", "
			<< legacy.y << ", " << legacy.z << "], \"scale\": [1, 1, 1] } } } } }";
		Scene old("Old");
		REQUIRE(old.deserialize(old_path));
		CHECK(same_rotation(find(old, "Thing").get_component<TransformComponent>().get_local_rotation(), legacy));
	}

	TEST_CASE("lights and cameras save in physical units, old light values get scaled to look the same at EV 15") {
		const std::string path = scene_dir() + "/lights.aqscene";
		{
			Scene scene("Saved");
			Entity sun = scene.get_entity_manager()->create_entity("Sun");
			sun.add_component<LightComponent>(LightComponent::Type::Directional, Vec3(1.F), 50000.F);
			Entity eye = scene.get_entity_manager()->create_entity("Eye");
			eye.add_component<CameraComponent>().ev100 = 9.F;
			REQUIRE(scene.serialize(path));
		}
		Scene loaded("Loaded");
		REQUIRE(loaded.deserialize(path));
		CHECK(find(loaded, "Sun").get_component<LightComponent>().get_intensity() == doctest::Approx(50000.F));
		CHECK(find(loaded, "Eye").get_component<CameraComponent>().ev100 == doctest::Approx(9.F));

		const std::string old_path = scene_dir() + "/old_lights.aqscene";
		std::ofstream(std::filesystem::temp_directory_path() / "aquila-mesh-tests" / "old_lights.aqscene")
			<< "{ \"scene_name\": \"Old\", \"entities\": {"
			   " \"0\": { \"components\": { \"metadata_component\": { \"name\": \"Sun\", \"uuid\": \""
			<< Foundation::UUID::generate().to_string()
			<< "\" }, \"light_component\": { \"type\": 1, \"intensity\": 1 } } },"
			   " \"1\": { \"components\": { \"metadata_component\": { \"name\": \"Bulb\", \"uuid\": \""
			<< Foundation::UUID::generate().to_string()
			<< "\" }, \"light_component\": { \"type\": 0, \"intensity\": 1 } } } } }";
		Scene old("Old");
		REQUIRE(old.deserialize(old_path));
		const F32 scale = 1.2F * std::exp2(15.F);
		CHECK(find(old, "Sun").get_component<LightComponent>().get_intensity() == doctest::Approx(scale));
		CHECK(find(old, "Bulb").get_component<LightComponent>().get_intensity() == doctest::Approx(scale * 4.F * Math::PI));
	}

	TEST_CASE("script field overrides and entity references survive a save, a load and a copy") {
		register_script_component();
		const std::string path = scene_dir() + "/script_fields.aqscene";
		Foundation::UUID target_id;
		{
			Scene scene("Saved");
			Entity target = scene.get_entity_manager()->create_entity("Target");
			target_id = target.get_component<MetadataComponent>().get_id();
			Entity scripted = scene.get_entity_manager()->create_entity("Scripted");
			auto &script = scripted.add_component<ScriptComponent>(ScriptComponent{ .path = "/project/Content/a.luau" });
			script.fields["speed"] = 7.5;
			script.fields["alive"] = false;
			script.fields["label"] = std::string("boss");
			script.fields["offset"] = Vec3(1.F, 2.F, 3.F);
			script.fields["target"] = EntityReference{ target_id };
			REQUIRE(scene.serialize(path));
		}

		Scene loaded("Loaded");
		REQUIRE(loaded.deserialize(path));
		Scene copy;
		copy.copy_from(loaded);
		for (Scene *scene : { &loaded, &copy }) {
			Entity scripted = find(*scene, "Scripted");
			REQUIRE(scripted.is_valid());
			const auto &script = scripted.get_component<ScriptComponent>();
			CHECK(script.path == "/project/Content/a.luau");
			REQUIRE(script.fields.size() == 5);
			CHECK(std::get<F64>(script.fields.at("speed")) == doctest::Approx(7.5));
			CHECK_FALSE(std::get<bool>(script.fields.at("alive")));
			CHECK(std::get<std::string>(script.fields.at("label")) == "boss");
			CHECK(std::get<Vec3>(script.fields.at("offset")) == Vec3(1.F, 2.F, 3.F));
			CHECK(std::get<EntityReference>(script.fields.at("target")).id == target_id);
			CHECK(find(*scene, "Target").get_component<MetadataComponent>().get_id() == target_id);
		}
	}

	TEST_CASE("a scene saves the mesh source and loads it back") {
		const std::string path = scene_dir() + "/round_trip.aqscene";
		{
			Scene scene("Saved");
			Entity ball = scene.get_entity_manager()->create_entity("Ball");
			auto &mesh = ball.add_component<MeshComponent>();
			mesh.set_mesh(MeshLibrary::instance().load(MeshSource::procedural("sphere", { { "segments", 8.F } })));
			mesh.receive_shadows = false;
			Entity empty = scene.get_entity_manager()->create_entity("Empty");
			empty.add_component<MeshComponent>();
			REQUIRE(scene.serialize(path));
		}

		Scene loaded("Loaded");
		REQUIRE(loaded.deserialize(path));
		Entity ball = find(loaded, "Ball");
		REQUIRE(ball.is_valid());
		const auto &mesh = ball.get_component<MeshComponent>();
		REQUIRE(mesh.is_valid());
		CHECK(mesh.data->get_source().generator == "sphere");
		CHECK(mesh.data->get_source().params.at("segments") == doctest::Approx(8.F));
		CHECK_FALSE(mesh.receive_shadows);

		Entity empty = find(loaded, "Empty");
		REQUIRE(empty.is_valid());
		CHECK(empty.has_component<MeshComponent>());
		CHECK_FALSE(empty.get_component<MeshComponent>().is_valid());
	}

	TEST_CASE("scenes saved in the old PascalCase format still load") {
		const std::string path = scene_dir() + "/legacy.aqscene";
		REQUIRE(Platform::Filesystem::VirtualFileSystem::get()->write_text_file(path, R"({
			"SceneName": "Old",
			"Entities": {
				"0": {
					"MetadataComponent": { "Name": "Box", "UUID": "11095055-b2be-45ed-a1d9-3e0b596ae123",
										   "Enabled": true, "Selected": false },
					"TransformComponent": { "Position": [1.0, 2.0, 3.0], "Rotation": [0.0, 0.0, 0.0],
											"Scale": [1.0, 1.0, 1.0] },
					"SceneNodeComponent": { "Parent": "null", "Children": [] },
					"MeshComponent": { "Path": "procedural://cube/0.5", "DebugName": "Box", "CastShadows": false }
				}
			}
		})"));

		Scene scene("Loaded");
		REQUIRE(scene.deserialize(path));
		CHECK(scene.get_scene_name() == "Old");
		Entity box = find(scene, "Box");
		REQUIRE(box.is_valid());
		const auto &mesh = box.get_component<MeshComponent>();
		REQUIRE(mesh.is_valid());
		CHECK(mesh.data->get_source().generator == "cube");
		CHECK(mesh.data->get_source().params.at("size") == doctest::Approx(1.F));
		CHECK_FALSE(mesh.cast_shadows);
	}

	TEST_CASE("broken scene files are rejected") {
		const std::string path = scene_dir() + "/broken.aqscene";
		REQUIRE(Platform::Filesystem::VirtualFileSystem::get()->write_text_file(path, "{ not json"));
		Scene scene("Loaded");
		CHECK_FALSE(scene.deserialize(path));
	}

	TEST_CASE("ids, visibility, materials and the primary camera survive a round trip") {
		const std::string path = scene_dir() + "/identity.aqscene";
		std::string box_id;
		{
			Scene scene("Saved");
			Entity box = scene.get_entity_manager()->create_entity("Box");
			box_id = box.get_component<MetadataComponent>().get_id().to_string();
			box.get_component<MetadataComponent>().set_visible(false);
			auto &material = box.add_component<MaterialComponent>();
			material.embedded.shader = "Custom/Toon.slang";
			material.embedded.values["roughness"] = 0.25F;
			Entity shared = scene.get_entity_manager()->create_entity("Shared");
			shared.add_component<MaterialComponent>().asset = "/project/Content/Brick.aqmat";
			Entity camera = scene.get_entity_manager()->create_entity("Camera");
			camera.add_component<CameraComponent>().primary = true;
			REQUIRE(scene.serialize(path));
		}

		Scene loaded("Loaded");
		bool notified = false;
		loaded.on_loaded.connect([&notified] { notified = true; });
		REQUIRE(loaded.deserialize(path));
		CHECK(notified);

		Entity box = find(loaded, "Box");
		REQUIRE(box.is_valid());
		CHECK(box.get_component<MetadataComponent>().get_id().to_string() == box_id);
		CHECK_FALSE(box.get_component<MetadataComponent>().is_visible());
		const auto &material = box.get_component<MaterialComponent>();
		CHECK(material.material == nullptr);
		CHECK_FALSE(material.uses_asset());
		CHECK(material.embedded.shader == "Custom/Toon.slang");
		REQUIRE(material.embedded.values.contains("roughness"));
		CHECK(std::get<F32>(material.embedded.values.at("roughness")) == doctest::Approx(0.25F));
		CHECK(find(loaded, "Shared").get_component<MaterialComponent>().asset == "/project/Content/Brick.aqmat");

		REQUIRE(loaded.has_active_camera());
		CHECK(loaded.get_active_camera_entity().get_component<MetadataComponent>().get_name() == "Camera");
	}

	TEST_CASE("loading twice keeps the entity names") {
		const std::string path = scene_dir() + "/names.aqscene";
		{
			Scene scene("Saved");
			scene.get_entity_manager()->create_entity("Lamp");
			REQUIRE(scene.serialize(path));
		}
		Scene loaded("Loaded");
		REQUIRE(loaded.deserialize(path));
		REQUIRE(loaded.deserialize(path));
		CHECK(find(loaded, "Lamp").is_valid());
		CHECK(loaded.get_registry().view<MetadataComponent>().size() == 1);
	}
}
