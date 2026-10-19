#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "Aquila/Platform/Filesystem/NativeFileSystem.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Platform/InputState.h"
#include "Aquila/Foundation/Log.h"
#include "Aquila/Scene/ComponentRegistry.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Scene/Components/SceneNodeComponent.h"
#include "Aquila/Scene/Components/ScriptComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scripting/ScriptSystem.h"

using namespace Aquila;
using namespace Aquila::SceneManagement;
using namespace Aquila::SceneManagement::Components;
using namespace Aquila::Scripting;

namespace {

std::string write_script(const std::string &name, const std::string &source) {
	static const std::filesystem::path dir = [] {
		auto path = std::filesystem::temp_directory_path() / "aquila-script-tests";
		std::filesystem::create_directories(path);
		Platform::Filesystem::VirtualFileSystem::init();
		Platform::Filesystem::VirtualFileSystem::get()->mount(
			"/script-tests", std::make_shared<Platform::Filesystem::NativeFileSystem>(path.string()));
		return path;
	}();
	std::ofstream(dir / name) << source;
	return "/script-tests/" + name;
}

Entity scripted_entity(Scene &scene, const std::string &path) {
	Entity entity = scene.get_entity_manager()->create_entity("Thing");
	entity.add_component<ScriptComponent>(ScriptComponent{ path });
	return entity;
}

} // namespace

TEST_SUITE("ScriptSystem") {
	TEST_CASE("the Script component is registered for the editor") {
		register_script_component();
		CHECK(ComponentRegistry::instance().find("Script") != nullptr);
	}

	TEST_CASE("the engine API types are written for luau-lsp") {
		auto directory = std::filesystem::temp_directory_path() / "aquila-script-types";
		Scene scene("Test");
		ScriptSystem system(scene);
		REQUIRE(system.write_types(directory.string()));

		auto read = [&](const char *name) {
			std::stringstream text;
			text << std::ifstream(directory / name).rdbuf();
			return text.str();
		};
		CHECK(read("globals.d.luau").find("set_position: (Entity, vector) -> ()") != std::string::npos);
		CHECK(read("native.luau").find("log: (string) -> ()") != std::string::npos);
	}

	TEST_CASE("scripts get the Luau API but not the raw bindings") {
		std::string path = write_script("api.luau", R"(
			local Aquila = require("@aquila")
			local M = {}
			function M.on_start(self)
				Aquila.log("hello")
				local raw = pcall(function() return require("@native") end)
				self:set_name(if raw then "raw" else "api")
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene);

		system.update(0.F);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "api");
	}

	TEST_CASE("on_start runs once and on_update every frame") {
		std::string path = write_script("drive.luau", R"(
			local M = {}
			function M.on_start(self)
				self:set_name("started")
				self:set_position(vector.create(1, 2, 3))
			end
			function M.on_update(self, dt)
				self:set_scale(self:get_scale() + vector.create(dt, dt, dt))
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene);

		system.update(0.5F);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "started");
		CHECK(entity.get_component<TransformComponent>().get_local_position() == Vec3{ 1.F, 2.F, 3.F });
		CHECK(entity.get_component<TransformComponent>().get_local_scale().x == doctest::Approx(1.5F));

		entity.get_component<MetadataComponent>().set_name("renamed");
		system.update(0.5F);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "renamed");
		CHECK(entity.get_component<TransformComponent>().get_local_scale().x == doctest::Approx(2.F));
	}

	TEST_CASE("changing the path loads the new script") {
		std::string first = write_script("first.luau", R"(
			local M = {}
			function M.on_start(self) self:set_name("first") end
			return M
		)");
		std::string second = write_script("second.luau", R"(
			local M = {}
			function M.on_start(self) self:set_name("second") end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, first);
		ScriptSystem system(scene);

		system.update(0.F);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "first");

		entity.get_component<ScriptComponent>().path = second;
		system.update(0.F);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "second");
	}

	TEST_CASE("each entity gets its own script instance") {
		std::string path = write_script("counter.luau", R"(
			local M = {}
			local count = 0
			function M.on_update(self, dt)
				count += 1
				self:set_position(vector.create(count, 0, 0))
			end
			return M
		)");
		Scene scene("Test");
		Entity a = scripted_entity(scene, path);
		ScriptSystem system(scene);
		system.update(0.F);
		system.update(0.F);
		Entity b = scripted_entity(scene, path);
		system.update(0.F);

		CHECK(a.get_component<TransformComponent>().get_local_position().x == doctest::Approx(3.F));
		CHECK(b.get_component<TransformComponent>().get_local_position().x == doctest::Approx(1.F));
	}

	TEST_CASE("removing the component or the entity stops the script") {
		std::string path = write_script("move.luau", R"(
			local M = {}
			function M.on_update(self, dt)
				self:set_position(self:get_position() + vector.create(1, 0, 0))
			end
			return M
		)");
		Scene scene("Test");
		Entity kept = scripted_entity(scene, path);
		Entity destroyed = scripted_entity(scene, path);
		ScriptSystem system(scene);

		system.update(0.F);
		kept.remove_component<ScriptComponent>();
		scene.get_entity_manager()->destroy_entity(destroyed);
		system.update(0.F);

		CHECK(kept.get_component<TransformComponent>().get_local_position().x == doctest::Approx(1.F));
	}

	TEST_CASE("a broken script does not stop the others") {
		std::string broken = write_script("broken.luau", R"(
			local M = {}
			function M.on_update(self, dt) error("boom") end
			return M
		)");
		std::string working = write_script("working.luau", R"(
			local M = {}
			function M.on_update(self, dt)
				self:set_position(self:get_position() + vector.create(1, 0, 0))
			end
			return M
		)");
		Scene scene("Test");
		scripted_entity(scene, broken);
		scripted_entity(scene, "/script-tests/missing.luau");
		Entity entity = scripted_entity(scene, working);
		ScriptSystem system(scene);

		system.update(0.F);
		system.update(0.F);
		CHECK(entity.get_component<TransformComponent>().get_local_position().x == doctest::Approx(2.F));
	}

	TEST_CASE("stop() runs on_stop once per running script, the next update starts them again") {
		std::string path = write_script("lifetime.luau", R"(
			local M = {}
			function M.on_start(self)
				self:set_name("started")
			end
			function M.on_stop(self)
				self:set_name("stopped")
				self:set_position(self:get_position() + vector.create(1, 0, 0))
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene);
		auto x = [&entity] { return entity.get_component<TransformComponent>().get_local_position().x; };

		system.update(0.F);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "started");

		system.stop();
		CHECK(entity.get_component<MetadataComponent>().get_name() == "stopped");
		CHECK(x() == doctest::Approx(1.F));

		system.stop();
		CHECK(x() == doctest::Approx(1.F));

		system.update(0.F);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "started");
	}

	TEST_CASE("on_stop runs when the script is removed or replaced, not when its entity is destroyed") {
		std::string path = write_script("stop-moves.luau", R"(
			local M = {}
			function M.on_stop(self)
				self:set_position(self:get_position() + vector.create(1, 0, 0))
			end
			return M
		)");
		std::string other = write_script("empty.luau", "return {}");
		Scene scene("Test");
		Entity removed = scripted_entity(scene, path);
		Entity replaced = scripted_entity(scene, path);
		Entity destroyed = scripted_entity(scene, path);
		ScriptSystem system(scene);
		auto x = [](Entity entity) { return entity.get_component<TransformComponent>().get_local_position().x; };

		system.update(0.F);
		removed.remove_component<ScriptComponent>();
		replaced.get_component<ScriptComponent>().path = other;
		scene.get_entity_manager()->destroy_entity(destroyed);
		system.update(0.F);

		CHECK(x(removed) == doctest::Approx(1.F));
		CHECK(x(replaced) == doctest::Approx(1.F));

		system.stop();
		CHECK(x(replaced) == doctest::Approx(1.F));
	}

	TEST_CASE("Aquila.Input reads the game input, Shift matches either side") {
		std::string path = write_script("input.luau", R"(
			local Input = require("@aquila").Input
			local M = {}
			function M.on_update(self, dt)
				local move = Input.get_vector("A", "D", "S", "W")
				self:set_position(move)
				local name = ""
				if Input.is_key_just_pressed("Space") then name ..= "jump " end
				if Input.is_key_down("Shift") then name ..= "run " end
				if Input.is_mouse_button_down("Left") then name ..= "fire " end
				self:set_name(name)
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		Platform::InputState input;
		ScriptSystem system(scene, &input);
		auto feed = [&input](Platform::Events::Event &&event) { input.on_event(event); };
		auto position = [&entity] { return entity.get_component<TransformComponent>().get_local_position(); };

		feed(Platform::Events::KeyPressedEvent(Platform::Events::KeyCode::W, 0));
		feed(Platform::Events::KeyPressedEvent(Platform::Events::KeyCode::D, 0));
		feed(Platform::Events::KeyPressedEvent(Platform::Events::KeyCode::Space, 0));
		feed(Platform::Events::KeyPressedEvent(Platform::Events::KeyCode::RightShift, 0));
		feed(Platform::Events::MouseButtonPressedEvent(Platform::Events::MouseButton::Left));
		system.update(0.F);

		CHECK(entity.get_component<MetadataComponent>().get_name() == "jump run fire ");
		CHECK(position().x == doctest::Approx(0.7071F).epsilon(0.001));
		CHECK(position().y == doctest::Approx(0.7071F).epsilon(0.001));

		input.end_frame();
		system.update(0.F);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "run fire ");
	}

	TEST_CASE("an unknown key name is a script error") {
		std::string path = write_script("typo.luau", R"(
			local Input = require("@aquila/Input")
			local M = {}
			function M.on_start(self)
				Input.is_key_down("Spcae")
				self:set_name("reached")
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		Platform::InputState input;
		ScriptSystem system(scene, &input);

		system.update(0.F);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "Thing");
	}

	TEST_CASE("Aquila.Screen and the mouse follow the game input's view") {
		std::string path = write_script("screen.luau", R"(
			local Aquila = require("@aquila")
			local M = {}
			function M.on_update(self, dt)
				local size = Aquila.Screen.get_size()
				local mouse = Aquila.Input.get_mouse_position()
				self:set_position(vector.create(size.x, size.y, 0))
				self:set_scale(vector.create(mouse.x, mouse.y, 1))
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		Platform::InputState input;
		input.set_view(Vec2(10.F, 20.F), Vec2(640.F, 480.F));
		Platform::Events::MouseMovedEvent moved(110.F, 70.F);
		input.on_event(moved);
		ScriptSystem system(scene, &input);

		system.update(0.F);
		const auto &transform = entity.get_component<TransformComponent>();
		CHECK(transform.get_local_position() == Vec3(640.F, 480.F, 0.F));
		CHECK(transform.get_local_scale() == Vec3(100.F, 50.F, 1.F));
	}

	TEST_CASE("Aquila.Time counts the seconds and frames played") {
		std::string path = write_script("time.luau", R"(
			local Time = require("@aquila").Time
			local M = {}
			function M.on_update(self, dt)
				self:set_position(vector.create(Time.get_elapsed(), Time.get_frame(), Time.get_delta()))
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene);

		system.update(0.5F);
		system.update(0.25F);
		const Vec3 position = entity.get_component<TransformComponent>().get_local_position();
		CHECK(position.x == doctest::Approx(0.75F));
		CHECK(position.y == doctest::Approx(2.F));
		CHECK(position.z == doctest::Approx(0.25F));
	}

	TEST_CASE("Aquila.Scene finds, creates and destroys entities") {
		std::string path = write_script("scene.luau", R"(
			local Scene = require("@aquila").Scene
			local M = {}
			function M.on_start(self)
				local target = Scene.find("Target")
				assert(target ~= nil and Scene.find("Nobody") == nil)
				Scene.destroy(target)
				local made = Scene.create("Made")
				made:set_position(vector.create(1, 2, 3))
				self:set_name(if target:is_valid() then "queued" else "gone")
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		Entity target = scene.get_entity_manager()->create_entity("Target");
		ScriptSystem system(scene);

		system.update(0.F);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "queued");
		scene.get_entity_manager()->flush_deletion_queue();

		CHECK_FALSE(target.exists());
		const auto made = scene.get_entity_manager()->find_entity_by_name("Made");
		REQUIRE(made.has_value());
		CHECK(made->get_component<TransformComponent>().get_local_position() == Vec3(1.F, 2.F, 3.F));
	}

	TEST_CASE("Entity methods: ids, parent, world position, forward and visibility") {
		std::string path = write_script("entity.luau", R"(
			local Scene = require("@aquila").Scene
			local M = {}
			function M.on_start(self)
				local parent = self:get_parent()
				assert(parent ~= nil and parent:get_parent() == nil)
				assert(parent:get_id() == Scene.find("Parent"):get_id())
				assert(parent:get_id() ~= self:get_id())
				local out = Scene.find("Out")
				out:set_position(self:get_forward())
				out:set_scale(self:get_world_position())
				self:set_visible(false)
				self:set_name(if self:is_visible() then "visible" else "hidden")
			end
			return M
		)");
		Scene scene("Test");
		EntityManager &entities = *scene.get_entity_manager();
		Entity parent = entities.create_entity("Parent");
		Entity entity = scripted_entity(scene, path);
		Entity out = entities.create_entity("Out");
		entities.add_child(parent, entity);
		parent.get_component<TransformComponent>().set_local_position(Vec3(1.F, 0.F, 0.F));
		entity.get_component<TransformComponent>().set_local_position(Vec3(0.F, 2.F, 0.F));
		entity.get_component<TransformComponent>().set_local_rotation_euler(Vec3(0.F, 90.F, 0.F));
		scene.update_transform_hierarchy();
		ScriptSystem system(scene);

		system.update(0.F);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "hidden");
		const auto &result = out.get_component<TransformComponent>();
		CHECK(result.get_local_position().x == doctest::Approx(1.F));
		CHECK(result.get_local_position().z == doctest::Approx(0.F).epsilon(0.0001));
		CHECK(result.get_local_scale() == Vec3(1.F, 2.F, 0.F));
	}

	TEST_CASE("script errors go to the engine log, with a traceback down to the script's line") {
		std::string path = write_script("fails.luau", R"(
			local Input = require("@aquila/Input")
			local M = {}
			function M.on_start(self)
				Input.is_key_down("Nope")
			end
			return M
		)");
		Scene scene("Test");
		scripted_entity(scene, path);
		ScriptSystem system(scene);

		std::ostringstream log;
		Foundation::Logger::set_sink(&log);
		system.update(0.F);
		Foundation::Logger::set_sink(nullptr);

		const std::string text = log.str();
		CHECK(text.find("[Luau]") != std::string::npos);
		CHECK(text.find("unknown key 'Nope'") != std::string::npos);
		CHECK(text.find("stack traceback:") != std::string::npos);
		CHECK(text.find("/script-tests/fails.luau:5 function on_start") != std::string::npos);
		CHECK(text.find("function traceback") == std::string::npos);
	}

	TEST_CASE("Aquila.Camera reads and changes cameras and maps between world and screen") {
		std::string path = write_script("camera.luau", R"(
			local Aquila = require("@aquila")
			local Camera = Aquila.Camera
			local M = {}
			function M.on_start(self)
				local camera = Camera.get_active()
				assert(camera ~= nil and camera:get_id() == Aquila.Scene.find("Eye"):get_id())
				Camera.set_fov(500)
				assert(Camera.get_fov() == 179)
				Camera.set_fov(60, camera)
				Camera.set_near(0.5)
				Camera.set_far(50)
				local ahead = Camera.world_to_screen(vector.create(0, 0, 10))
				assert(ahead ~= nil)
				assert(Camera.world_to_screen(vector.create(0, 0, -10)) == nil)
				local origin, direction = Camera.screen_to_ray(vector.create(100, 50, 0))
				self:set_position(ahead)
				self:set_scale(direction)
				self:set_name(if pcall(Camera.get_fov, self) then "no error" else "error")
				Camera.set_active(Aquila.Scene.find("Other"))
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		auto make_camera = [&scene](const std::string &name) {
			Entity camera = scene.get_entity_manager()->create_entity(name);
			auto &lens = camera.add_component<CameraComponent>();
			lens.aspect_ratio = 2.F;
			return camera;
		};
		Entity eye = make_camera("Eye");
		Entity other = make_camera("Other");
		scene.set_active_camera(eye);
		scene.update_transform_hierarchy();
		Platform::InputState input;
		input.set_view(Vec2(0.F), Vec2(200.F, 100.F));
		ScriptSystem system(scene, &input);

		system.update(0.F);
		const auto &lens = eye.get_component<CameraComponent>();
		CHECK(lens.fov == doctest::Approx(60.F));
		CHECK(lens.near_plane == doctest::Approx(0.5F));
		CHECK(lens.far_plane == doctest::Approx(50.F));
		const auto &transform = entity.get_component<TransformComponent>();
		CHECK(transform.get_local_position().x == doctest::Approx(100.F));
		CHECK(transform.get_local_position().y == doctest::Approx(50.F));
		CHECK(transform.get_local_scale().z == doctest::Approx(1.F));
		CHECK(entity.get_component<MetadataComponent>().get_name() == "error");
		CHECK(scene.get_active_camera_entity() == other);
	}

	TEST_CASE("renaming and hiding from a script tell the scene, so the hierarchy follows") {
		std::string path = write_script("rename.luau", R"(
			local M = {}
			function M.on_start(self)
				self:set_name("Renamed")
				self:set_visible(false)
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene);
		int changes = 0;
		scene.on_entities_changed.connect([&changes] { ++changes; });

		system.update(0.F);
		CHECK(changes == 2);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "Renamed");
		CHECK_FALSE(entity.get_component<MetadataComponent>().is_visible());
	}

}
