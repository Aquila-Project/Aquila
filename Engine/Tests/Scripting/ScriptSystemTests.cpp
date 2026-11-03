#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "Aquila/Graphics/Resources/MeshLibrary.h"
#include "Aquila/Platform/Filesystem/NativeFileSystem.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Platform/InputState.h"
#include "Aquila/Foundation/Log.h"
#include "Aquila/Scene/ComponentRegistry.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Components/FogComponent.h"
#include "Aquila/Scene/Components/LightComponent.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Scene/Components/SceneNodeComponent.h"
#include "Aquila/Scene/Components/ScriptComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scripting/ScriptApi.h"
#include "Aquila/Scripting/ScriptFields.h"
#include "Aquila/Scripting/ScriptSystem.h"

using namespace Aquila;
using namespace Aquila::SceneManagement;
using namespace Aquila::SceneManagement::Components;
using namespace Aquila::Scripting;

namespace {

const std::filesystem::path &script_dir() {
	static const std::filesystem::path dir = [] {
		auto path = std::filesystem::temp_directory_path() / "aquila-script-tests";
		std::filesystem::create_directories(path);
		Platform::Filesystem::VirtualFileSystem::init();
		Platform::Filesystem::VirtualFileSystem::get()->mount(
			"/script-tests", std::make_shared<Platform::Filesystem::NativeFileSystem>(path.string()));
		return path;
	}();
	return dir;
}

std::string write_script(const std::string &name, const std::string &source) {
	std::ofstream(script_dir() / name) << source;
	return "/script-tests/" + name;
}

struct PluginState {
	int calls = 0;
};

int plugin_answer(PluginState &state) {
	++state.calls;
	return 42;
}

std::string plugin_api_dir() {
	static const std::filesystem::path dir = [] {
		auto path = script_dir() / "plugin-api";
		std::filesystem::create_directories(path);
		std::ofstream(path / "init.luau") << R"(
			local native = require("@plugintest_native")
			local Plugin = {}
			function Plugin.answer(): number
				return native.answer()
			end
			return Plugin
		)";
		return path;
	}();
	return dir.string();
}

struct TintComponent {
	Vec4 tint{ 1.F };
};

void register_tint_component() {
	static const bool registered = [] {
		ComponentRegistry::instance()
			.register_component<TintComponent>("Test Tint")
			.property("Tint", &TintComponent::tint, { .color = true });
		return true;
	}();
	(void)registered;
}

Vec3 world_axis(Entity entity, Vec3 axis) {
	return Math::normalize(Mat3(entity.get_component<TransformComponent>().get_world_matrix()) * axis);
}

void check_near(Vec3 actual, Vec3 expected) {
	CAPTURE(actual);
	CAPTURE(expected);
	CHECK(Math::length(actual - expected) < 0.001F);
}

Foundation::Registration add_plugin(ScriptApi &api, PluginState &state) {
	return api.add([&state](aquiLua::Runtime &runtime, ScriptEnvironment &) {
		runtime.bind<plugin_answer>("plugintest_native.answer", &state, "Always 42.");
		runtime.add_alias("plugintest", plugin_api_dir());
	});
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

	TEST_CASE("destroying an entity from a script runs on_stop for it and its children, once") {
		std::string bullet = write_script("bullet.luau", R"(
			local Scene = require("@aquila").Scene
			local log
			local M = {}
			function M.on_start(self)
				log = Scene.find("Log")
			end
			function M.on_stop(self)
				log:set_name(log:get_name() .. "|" .. self:get_name())
			end
			return M
		)");
		std::string killer = write_script("killer.luau", R"(
			local Scene = require("@aquila").Scene
			local M = {}
			function M.on_update(self, dt)
				local bullet = Scene.find("Bullet")
				if bullet then
					Scene.destroy(bullet)
				end
			end
			return M
		)");
		Scene scene("Test");
		EntityManager &entities = *scene.get_entity_manager();
		Entity log = entities.create_entity("Log");
		Entity shot = scripted_entity(scene, bullet);
		shot.get_component<MetadataComponent>().set_name("Bullet");
		Entity spark = scripted_entity(scene, bullet);
		spark.get_component<MetadataComponent>().set_name("Spark");
		entities.add_child(shot, spark);
		scripted_entity(scene, killer);
		ScriptSystem system(scene);

		system.update(0.F);
		entities.flush_deletion_queue();
		system.update(0.F);
		system.stop();
		const std::string &text = log.get_component<MetadataComponent>().get_name();
		CHECK((text == "Log|Bullet|Spark" || text == "Log|Spark|Bullet"));
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

	TEST_CASE("vectors have a fourth component, for colors with alpha") {
		std::string path = write_script("vector4.luau", R"(
			local M = {}
			function M.on_start(self)
				local color = vector.create(1, 0.5, 0.25, 0.75)
				local plain = vector.create(1, 2, 3)
				self:set_name(tostring(color.w) .. " " .. tostring(plain.w))
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene);

		system.update(0.F);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "0.75 0");
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

	TEST_CASE("Aquila.Field declares fields in order, with types, defaults and hints, without running hooks") {
		std::string path = write_script("fields.luau", R"(
			local Aquila = require("@aquila")
			local Field = Aquila.Field
			local speed = Field.number("speed", 5, { min = 0, max = 20, slider = true })
			local count = Field.number("count", 3, { integer = true })
			local alive = Field.boolean("alive", true)
			local label = Field.string("label", "hi")
			local offset = Field.vector("offset", vector.create(0, 1, 0))
			local tint = Field.color("tint", vector.create(1, 0.5, 0))
			local target = Field.entity("target")
			Field.number("speed", 99)
			Aquila.Scene.create("Junk")
			local M = {}
			function M.on_start(self)
				error("hooks don't run while reading fields")
			end
			return M
		)");

		const std::vector<ScriptField> fields = read_script_fields(path);
		REQUIRE(fields.size() == 7);
		CHECK(fields[0].name == "speed");
		CHECK(fields[0].type == ScriptFieldType::Number);
		CHECK(std::get<F64>(fields[0].default_value) == doctest::Approx(5.0));
		CHECK(fields[0].min == doctest::Approx(0.0));
		CHECK(fields[0].max == doctest::Approx(20.0));
		CHECK(fields[0].slider);
		CHECK(fields[1].integer);
		CHECK_FALSE(std::isfinite(fields[1].max));
		CHECK(fields[2].type == ScriptFieldType::Boolean);
		CHECK(std::get<std::string>(fields[3].default_value) == "hi");
		CHECK(std::get<Vec3>(fields[4].default_value) == Vec3(0.F, 1.F, 0.F));
		CHECK(fields[5].color);
		CHECK(fields[6].type == ScriptFieldType::Entity);
		CHECK(std::get<EntityReference>(fields[6].default_value).id == Foundation::UUID::null());
		CHECK(read_script_fields("/script-tests/nope.luau").empty());
	}

	TEST_CASE("each entity gets its own field values, entity fields resolve by UUID") {
		std::string path = write_script("field-values.luau", R"(
			local Field = require("@aquila/Field")
			local speed = Field.number("speed", 5, { max = 10 })
			local count = Field.number("count", 1, { integer = true })
			local target = Field.entity("target")
			local M = {}
			function M.on_start(self)
				self:set_position(vector.create(speed, count, 0))
				self:set_name(if target then "-> " .. target:get_name() else "no target")
			end
			return M
		)");
		Scene scene("Test");
		Entity target = scene.get_entity_manager()->create_entity("Target");
		Entity tuned = scripted_entity(scene, path);
		Entity plain = scripted_entity(scene, path);
		Entity broken = scripted_entity(scene, path);
		auto &fields = tuned.get_component<ScriptComponent>().fields;
		fields["speed"] = 50.0;
		fields["count"] = 2.6;
		fields["target"] = EntityReference{ target.get_component<MetadataComponent>().get_id() };
		auto &wrong = broken.get_component<ScriptComponent>().fields;
		wrong["speed"] = std::string("fast");
		wrong["target"] = EntityReference{ Foundation::UUID::generate() };
		ScriptSystem system(scene);

		system.update(0.F);
		auto position = [](Entity entity) { return entity.get_component<TransformComponent>().get_local_position(); };
		CHECK(position(tuned) == Vec3(10.F, 3.F, 0.F));
		CHECK(tuned.get_component<MetadataComponent>().get_name() == "-> Target");
		CHECK(position(plain) == Vec3(5.F, 1.F, 0.F));
		CHECK(plain.get_component<MetadataComponent>().get_name() == "no target");
		CHECK(position(broken) == Vec3(5.F, 1.F, 0.F));
		CHECK(broken.get_component<MetadataComponent>().get_name() == "no target");
	}

	TEST_CASE("entities can be found by the UUID scenes save") {
		std::string path = write_script("uuid.luau", R"(
			local Scene = require("@aquila").Scene
			local M = {}
			function M.on_start(self)
				local found = Scene.find_by_uuid(self:get_uuid())
				assert(found ~= nil and found:get_id() == self:get_id())
				assert(Scene.find_by_uuid("not a uuid") == nil)
				self:set_name(self:get_uuid())
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene);

		system.update(0.F);
		const auto &metadata = entity.get_component<MetadataComponent>();
		CHECK(metadata.get_name() == metadata.get_id().to_string());
	}
}

TEST_SUITE("ScriptApi") {
	TEST_CASE("an added binder reaches scripts through its own Luau module, next to the engine's") {
		PluginState state;
		ScriptApi api;
		Foundation::Registration plugin = add_plugin(api, state);
		std::string path = write_script("plugin.luau", R"(
			local Plugin = require("@plugintest")
			local M = {}
			function M.on_start(self)
				self:set_name(tostring(Plugin.answer()))
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene, nullptr, &api);

		system.update(0.F);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "42");
		CHECK(state.calls == 1);
	}

	TEST_CASE("binders run in the order they were added") {
		ScriptApi api;
		std::vector<int> order;
		Foundation::Registration first =
			api.add([&order](aquiLua::Runtime &, ScriptEnvironment &) { order.push_back(1); });
		Foundation::Registration second =
			api.add([&order](aquiLua::Runtime &, ScriptEnvironment &) { order.push_back(2); });
		Scene scene("Test");
		ScriptSystem system(scene, nullptr, &api);
		CHECK(order == std::vector<int>{ 1, 2 });
	}

	TEST_CASE("a removed binder is gone from the next runtime, and the revision moves") {
		ScriptApi api;
		int runs = 0;
		const Uint64 empty = api.get_revision();
		Foundation::Registration added = api.add([&runs](aquiLua::Runtime &, ScriptEnvironment &) { ++runs; });
		const Uint64 with = api.get_revision();
		CHECK(with != empty);
		added.reset();
		CHECK(api.get_revision() != with);

		Scene scene("Test");
		ScriptSystem system(scene, nullptr, &api);
		CHECK(runs == 0);
	}

	TEST_CASE("the field reader and the type files see added modules") {
		PluginState state;
		ScriptApi api;
		Foundation::Registration plugin = add_plugin(api, state);
		std::string path = write_script("plugin-fields.luau", R"(
			local Plugin = require("@plugintest")
			local Field = require("@aquila/Field")
			Field.number("answer", Plugin.answer())
			return {}
		)");

		const std::vector<ScriptField> fields = read_script_fields(path, &api);
		REQUIRE(fields.size() == 1);
		CHECK(std::get<F64>(fields[0].default_value) == doctest::Approx(42.0));

		const auto directory = std::filesystem::temp_directory_path() / "aquila-plugin-types";
		Scene scene("Test");
		REQUIRE(ScriptSystem(scene, nullptr, &api).write_types(directory.string()));
		std::stringstream text;
		text << std::ifstream(directory / "plugintest_native.luau").rdbuf();
		CHECK(text.str().find("answer") != std::string::npos);
	}
}

TEST_SUITE("Script components") {
	TEST_CASE("components are read and written by name through their properties") {
		std::string path = write_script("components.luau", R"(
			local M = {}
			function M.on_start(self)
				self:set("Light", "Type", "Point")
				self:set("Light", "Color", vector.create(1, 0.5, 0))
				self:set("Light", "Range", 7)
				self:set("Light", "Cast Shadows", false)
				self:set_name(self:get("Light", "Type") .. " " .. tostring(self:get("Light", "Range")))
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		entity.add_component<LightComponent>();
		ScriptSystem system(scene);

		system.update(0.F);
		const auto &light = entity.get_component<LightComponent>();
		CHECK(light.get_type() == LightComponent::Type::Point);
		CHECK(light.get_color() == Vec3(1.F, 0.5F, 0.F));
		CHECK(light.get_range() == doctest::Approx(7.F));
		CHECK_FALSE(light.get_cast_shadows());
		CHECK(entity.get_component<MetadataComponent>().get_name() == "Point 7");
	}

	TEST_CASE("of properties sharing a name, the one the inspector shows wins") {
		std::string path = write_script("candela.luau", R"(
			local M = {}
			function M.on_start(self)
				self:set("Light", "Intensity", 100)
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		auto &light = entity.add_component<LightComponent>();
		light.set_type(LightComponent::Type::Point);
		light.set_intensity_unit(LightComponent::IntensityUnit::Candela);
		ScriptSystem system(scene);

		system.update(0.F);
		CHECK(light.get_candela() == doctest::Approx(100.F));
	}

	TEST_CASE("numbers are kept inside the property's range") {
		std::string path = write_script("fov.luau", R"(
			local M = {}
			function M.on_start(self)
				self:set("Camera", "FOV", 500)
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		entity.add_component<CameraComponent>();
		ScriptSystem system(scene);

		system.update(0.F);
		CHECK(entity.get_component<CameraComponent>().fov == doctest::Approx(179.F));
	}

	TEST_CASE("colors with alpha travel as four wide vectors") {
		register_tint_component();
		std::string path = write_script("tint.luau", R"(
			local M = {}
			function M.on_start(self)
				self:add("Test Tint")
				self:set("Test Tint", "Tint", vector.create(1, 0.5, 0.25, 0.75))
				self:set_name(tostring(self:get("Test Tint", "Tint").w))
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene);

		system.update(0.F);
		REQUIRE(entity.has_component<TintComponent>());
		CHECK(entity.get_component<TintComponent>().tint == Vec4(1.F, 0.5F, 0.25F, 0.75F));
		CHECK(entity.get_component<MetadataComponent>().get_name() == "0.75");
	}

	TEST_CASE("Aquila.Color makes colors with alpha, opaque unless told otherwise") {
		register_tint_component();
		std::string path = write_script("color.luau", R"(
			local Color = require("@aquila").Color
			local M = {}
			function M.on_start(self)
				self:add("Test Tint")
				self:set("Test Tint", "Tint", Color.new(1, 0.5, 0.25))
				local opaque = Color.alpha(self:get("Test Tint", "Tint"))
				local faded = Color.alpha(Color.new(1, 1, 1, 0.25))
				self:set_name(tostring(opaque) .. " " .. tostring(faded))
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene);

		system.update(0.F);
		CHECK(entity.get_component<TintComponent>().tint == Vec4(1.F, 0.5F, 0.25F, 1.F));
		CHECK(entity.get_component<MetadataComponent>().get_name() == "1 0.25");
	}

	TEST_CASE("has, add and remove follow what the editor allows") {
		std::string path = write_script("add-remove.luau", R"(
			local M = {}
			function M.on_start(self)
				assert(not self:has("Fog"))
				self:add("Fog")
				assert(self:has("Fog"))
				self:set("Fog", "Density", 0.1)
				self:remove("Camera")
				assert(not pcall(function() self:remove("Transform") end))
				self:set_name("done")
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		entity.add_component<CameraComponent>();
		ScriptSystem system(scene);

		system.update(0.F);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "done");
		REQUIRE(entity.has_component<FogComponent>());
		CHECK(entity.get_component<FogComponent>().density == doctest::Approx(0.1F));
		CHECK_FALSE(entity.has_component<CameraComponent>());
		CHECK(entity.has_component<TransformComponent>());
	}

	TEST_CASE("unknown names, wrong types and read-only properties are script errors") {
		std::string path = write_script("component-errors.luau", R"(
			local M = {}
			function M.on_start(self)
				local function fails(f)
					return not pcall(f)
				end
				assert(fails(function() return self:get("Lihgt", "Color") end), "unknown component")
				assert(fails(function() return self:get("Light", "Colour") end), "unknown property")
				assert(fails(function() return self:get("Light", "Color") end), "missing component")
				self:add("Light")
				assert(fails(function() self:set("Light", "Range", "far") end), "wrong type")
				assert(fails(function() self:set("Light", "Type", "Laser") end), "unknown option")
				assert(fails(function() self:set("Metadata", "UUID", "x") end), "read-only")
				self:set_name("all failed")
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene);

		system.update(0.F);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "all failed");
	}

	TEST_CASE("writes tell the scene, so the hierarchy follows") {
		std::string path = write_script("component-notify.luau", R"(
			local M = {}
			function M.on_start(self)
				self:set("Metadata", "Visible", false)
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene);
		int changes = 0;
		scene.on_entities_changed.connect([&changes] { ++changes; });

		system.update(0.F);
		CHECK(changes == 1);
		CHECK_FALSE(entity.get_component<MetadataComponent>().is_visible());
	}
}

TEST_SUITE("Script transforms") {
	TEST_CASE("set_world_position puts it there under a moved parent and reads back at once") {
		std::string path = write_script("world-position.luau", R"(
			local M = {}
			function M.on_start(self)
				self:get_parent():set_position(vector.create(1, 0, 0))
				self:set_world_position(vector.create(3, 4, 5))
				local at = self:get_world_position()
				assert(vector.magnitude(at - vector.create(3, 4, 5)) < 0.001, tostring(at))
				self:set_name("there")
			end
			return M
		)");
		Scene scene("Test");
		EntityManager &entities = *scene.get_entity_manager();
		Entity parent = entities.create_entity("Parent");
		Entity entity = scripted_entity(scene, path);
		entities.add_child(parent, entity);
		auto &parent_transform = parent.get_component<TransformComponent>();
		parent_transform.set_local_rotation_euler(Vec3(0.F, 90.F, 0.F));
		parent_transform.set_local_scale(Vec3(2.F));
		scene.update_transform_hierarchy();
		ScriptSystem system(scene);

		system.update(0.F);
		scene.update_transform_hierarchy();
		CHECK(entity.get_component<MetadataComponent>().get_name() == "there");
		check_near(entity.get_component<TransformComponent>().get_world_position(), Vec3(3.F, 4.F, 5.F));
	}

	TEST_CASE("forward, up and right follow a rotation made this frame") {
		std::string path = write_script("axes.luau", R"(
			local M = {}
			function M.on_start(self)
				self:set_rotation(vector.create(0, 90, 0))
				local out = require("@aquila").Scene.find("Out")
				out:set_position(self:get_forward())
				out:set_rotation(self:get_up())
				out:set_scale(self:get_right())
			end
			return M
		)");
		Scene scene("Test");
		scripted_entity(scene, path);
		Entity out = scene.get_entity_manager()->create_entity("Out");
		ScriptSystem system(scene);

		system.update(0.F);
		const auto &result = out.get_component<TransformComponent>();
		check_near(result.get_local_position(), Vec3(1.F, 0.F, 0.F));
		check_near(result.get_local_rotation_euler(), Vec3(0.F, 1.F, 0.F));
		check_near(result.get_local_scale(), Vec3(0.F, 0.F, -1.F));
	}

	TEST_CASE("look_at turns +Z toward the target and keeps +Y up, under a turned parent") {
		std::string path = write_script("look-at.luau", R"(
			local M = {}
			function M.on_start(self)
				self:look_at(vector.create(0, 0, -10))
			end
			return M
		)");
		Scene scene("Test");
		EntityManager &entities = *scene.get_entity_manager();
		Entity parent = entities.create_entity("Parent");
		Entity entity = scripted_entity(scene, path);
		entities.add_child(parent, entity);
		parent.get_component<TransformComponent>().set_local_rotation_euler(Vec3(0.F, 30.F, 0.F));
		scene.update_transform_hierarchy();
		ScriptSystem system(scene);

		system.update(0.F);
		scene.update_transform_hierarchy();
		check_near(world_axis(entity, Vec3(0.F, 0.F, 1.F)), Vec3(0.F, 0.F, -1.F));
		check_near(world_axis(entity, Vec3(0.F, 1.F, 0.F)), Vec3(0.F, 1.F, 0.F));
	}

	TEST_CASE("rotate turns around a world axis, whatever the parent's rotation") {
		std::string path = write_script("rotate.luau", R"(
			local M = {}
			function M.on_start(self)
				self:rotate(vector.create(0, 1, 0), 90)
			end
			return M
		)");
		Scene scene("Test");
		EntityManager &entities = *scene.get_entity_manager();
		Entity parent = entities.create_entity("Parent");
		Entity entity = scripted_entity(scene, path);
		entities.add_child(parent, entity);
		parent.get_component<TransformComponent>().set_local_rotation_euler(Vec3(90.F, 0.F, 0.F));
		scene.update_transform_hierarchy();
		ScriptSystem system(scene);

		system.update(0.F);
		scene.update_transform_hierarchy();
		check_near(world_axis(entity, Vec3(0.F, 1.F, 0.F)), Vec3(1.F, 0.F, 0.F));
	}

	TEST_CASE("set_parent keeps the world transform, get_children lists the children") {
		std::string path = write_script("reparent.luau", R"(
			local Scene = require("@aquila").Scene
			local M = {}
			function M.on_start(self)
				local holder = Scene.find("Holder")
				self:set_parent(holder)
				local children = holder:get_children()
				assert(#children == 1 and children[1]:get_id() == self:get_id())
				assert(self:get_parent():get_id() == holder:get_id())
				local below = Scene.find("Below")
				assert(not pcall(function() self:set_parent(self) end), "itself")
				assert(not pcall(function() self:set_parent(below) end), "its own child")
				self:set_parent(nil)
				assert(self:get_parent() == nil and #holder:get_children() == 0)
				self:set_parent(holder)
				self:set_name("moved")
			end
			return M
		)");
		Scene scene("Test");
		EntityManager &entities = *scene.get_entity_manager();
		Entity holder = entities.create_entity("Holder");
		Entity entity = scripted_entity(scene, path);
		Entity below = entities.create_entity("Below");
		entities.add_child(entity, below);
		auto &holder_transform = holder.get_component<TransformComponent>();
		holder_transform.set_local_position(Vec3(5.F, 0.F, 0.F));
		holder_transform.set_local_rotation_euler(Vec3(0.F, 90.F, 0.F));
		holder_transform.set_local_scale(Vec3(2.F));
		entity.get_component<TransformComponent>().set_local_position(Vec3(1.F, 2.F, 3.F));
		scene.update_transform_hierarchy();
		ScriptSystem system(scene);

		system.update(0.F);
		scene.update_transform_hierarchy();
		CHECK(entity.get_component<MetadataComponent>().get_name() == "moved");
		REQUIRE(entities.get_parent(entity).has_value());
		CHECK(*entities.get_parent(entity) == holder);
		const auto &transform = entity.get_component<TransformComponent>();
		check_near(transform.get_world_position(), Vec3(1.F, 2.F, 3.F));
		check_near(transform.get_world_scale(), Vec3(1.F));
		check_near(world_axis(entity, Vec3(0.F, 0.F, 1.F)), Vec3(0.F, 0.F, 1.F));
	}
}

TEST_SUITE("Aquila.Math") {
	TEST_CASE("Aquila.Math fills in what Luau's math and vector libraries don't have") {
		std::string path = write_script("math.luau", R"(
			local Math = require("@aquila").Math
			local function near(a: number, b: number, label: string)
				assert(math.abs(a - b) < 1e-4, label .. ": " .. tostring(a) .. " ~= " .. tostring(b))
			end
			local function near_vector(a: vector, b: vector, label: string)
				assert(vector.magnitude(a - b) < 1e-4, label .. ": " .. tostring(a) .. " ~= " .. tostring(b))
			end

			local M = {}
			function M.on_start(self)
				near(Math.smooth_step(0, 1, 0.5), 0.5, "smooth_step middle")
				near(Math.smooth_step(0, 1, 0.25), 0.15625, "smooth_step curve")
				near(Math.smooth_step(0, 1, 2), 1, "smooth_step clamps")
				near(Math.inverse_lerp(10, 20, 15), 0.5, "inverse_lerp")
				assert(Math.approx_equal(0.1 + 0.2, 0.3), "approx_equal")
				assert(not Math.approx_equal(1, 1.01), "approx_equal tolerance")
				near(Math.move_towards(0, 10, 3), 3, "move_towards step")
				near(Math.move_towards(9, 10, 3), 10, "move_towards no overshoot")
				near(Math.move_towards(5, -5, 2), 3, "move_towards down")
				near(Math.damp(Math.damp(0, 10, 4, 0.25), 10, 4, 0.25), Math.damp(0, 10, 4, 0.5), "damp frame rate")
				near(Math.angle_difference(350, 10), 20, "angle_difference wraps")
				near(Math.angle_difference(10, 350), -20, "angle_difference sign")

				local a = vector.create(0, 0, 0)
				local b = vector.create(3, 4, 0)
				near(Math.distance(a, b), 5, "distance")
				near_vector(Math.move_towards_vector(a, b, 1), vector.create(0.6, 0.8, 0), "move_towards_vector")
				near_vector(Math.move_towards_vector(a, b, 10), b, "move_towards_vector no overshoot")
				near_vector(Math.damp_vector(a, b, 1000, 1), b, "damp_vector settles")
				near_vector(Math.reflect(vector.create(1, -1, 0), vector.create(0, 1, 0)), vector.create(1, 1, 0), "reflect")
				near_vector(Math.project(vector.create(2, 3, 0), vector.create(5, 0, 0)), vector.create(2, 0, 0), "project")
				near_vector(Math.project_on_plane(vector.create(2, 3, 0), vector.create(0, 1, 0)), vector.create(2, 0, 0),
					"project_on_plane")

				local axis = vector.normalize(vector.create(1, 2, 3))
				self:rotate(axis, 70)
				near_vector(Math.rotate_vector(vector.create(0, 0, 1), axis, 70), self:get_forward(),
					"rotate_vector turns like Entity:rotate")

				near(Math.fbm(vector.create(1.3, 2.7, 0.4), 1), math.noise(1.3, 2.7, 0.4), "fbm with one octave")
				for i = 1, 50 do
					local value = Math.fbm(vector.create(i * 0.37, i * 0.11, 0))
					assert(value >= -1 and value <= 1, "fbm range " .. tostring(value))
					local roll = Math.random_range(-2, 3)
					assert(roll >= -2 and roll < 3, "random_range " .. tostring(roll))
					near(vector.magnitude(Math.random_unit_vector()), 1, "random_unit_vector")
				end
				self:set_name("math ok")
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene);

		system.update(0.F);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "math ok");
	}
}

TEST_SUITE("Aquila.Time timers") {
	TEST_CASE("after and every run on game time, cancel stops them") {
		std::string path = write_script("timers.luau", R"(
			local Time = require("@aquila").Time
			local fired = "none"
			local ticks = 0
			local M = {}
			function M.on_start(self)
				Time.after(1, function()
					fired = "after"
				end)
				local every
				every = Time.every(0.5, function()
					ticks += 1
					if ticks == 3 then
						every:cancel()
					end
				end)
				Time.after(0.2, function()
					error("a cancelled timer ran")
				end):cancel()
			end
			function M.on_update(self, dt)
				self:set_name(fired .. " " .. tostring(ticks))
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene);
		auto name = [&] { return entity.get_component<MetadataComponent>().get_name(); };

		std::ostringstream log;
		Foundation::Logger::set_sink(&log);
		for (int i = 0; i < 3; ++i) {
			system.update(0.25F);
		}
		system.update(0.F);
		CHECK(name() == "none 1");
		for (int i = 0; i < 6; ++i) {
			system.update(0.25F);
		}
		system.update(0.F);
		Foundation::Logger::set_sink(nullptr);
		CHECK(name() == "after 3");
		CHECK(log.str().find("cancelled timer") == std::string::npos);
	}

	TEST_CASE("spawn runs a sequence that waits between steps") {
		std::string path = write_script("sequence.luau", R"(
			local Time = require("@aquila").Time
			local M = {}
			function M.on_start(self)
				assert(not pcall(Time.wait, 1), "wait outside spawn")
				Time.spawn(function(label: string)
					self:set_name(label .. " 1")
					Time.wait(1)
					self:set_name(label .. " 2")
					Time.wait(0.5)
					self:set_name(label .. " 3")
				end, "step")
				Time.spawn(function()
					Time.wait(0.1)
					error("a cancelled sequence went on")
				end):cancel()
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene);
		auto name = [&] { return entity.get_component<MetadataComponent>().get_name(); };

		std::ostringstream log;
		Foundation::Logger::set_sink(&log);
		system.update(0.F);
		CHECK(name() == "step 1");
		system.update(0.5F);
		CHECK(name() == "step 1");
		system.update(0.5F);
		CHECK(name() == "step 2");
		system.update(0.5F);
		Foundation::Logger::set_sink(nullptr);
		CHECK(name() == "step 3");
		CHECK(log.str().find("cancelled sequence") == std::string::npos);
	}

	TEST_CASE("an error in a timer is logged with its traceback and the others still run") {
		std::string path = write_script("timer-error.luau", R"(
			local Time = require("@aquila").Time
			local M = {}
			function M.on_start(self)
				Time.after(0.1, function()
					error("timer went wrong")
				end)
				Time.after(0.1, function()
					self:set_name("still ran")
				end)
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene);

		std::ostringstream log;
		Foundation::Logger::set_sink(&log);
		system.update(0.2F);
		Foundation::Logger::set_sink(nullptr);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "still ran");
		const std::string text = log.str();
		CHECK(text.find("timer went wrong") != std::string::npos);
		CHECK(text.find("/script-tests/timer-error.luau:6") != std::string::npos);
	}
}

TEST_SUITE("Aquila.Events") {
	TEST_CASE("one script's emit reaches the handlers other scripts added") {
		std::string receiver = write_script("events-receiver.luau", R"(
			local Events = require("@aquila").Events
			local health = 100
			local M = {}
			function M.on_start(self)
				Events.on("damage", function(target: Entity, amount: number)
					if target:get_id() == self:get_id() then
						health -= amount
						self:set_name("health " .. tostring(health))
					end
				end)
			end
			return M
		)");
		std::string sender = write_script("events-sender.luau", R"(
			local Aquila = require("@aquila")
			local target
			local M = {}
			function M.on_start(self)
				target = Aquila.Scene.find("Target")
			end
			function M.on_update(self, dt)
				Aquila.Events.emit("damage", target, 5)
			end
			return M
		)");
		Scene scene("Test");
		Entity target = scripted_entity(scene, receiver);
		target.get_component<MetadataComponent>().set_name("Target");
		scripted_entity(scene, sender);
		ScriptSystem system(scene);

		system.update(0.F);
		system.update(0.F);
		CHECK(target.get_component<MetadataComponent>().get_name() == "health 90");
	}

	TEST_CASE("once, disconnect and a failing handler") {
		std::string path = write_script("events-rules.luau", R"(
			local Events = require("@aquila").Events
			local M = {}
			function M.on_start(self)
				local counts = { on = 0, once = 0, gone = 0, late = 0 }
				Events.on("ping", function()
					counts.on += 1
				end)
				Events.once("ping", function()
					counts.once += 1
				end)
				local gone = Events.on("ping", function()
					counts.gone += 1
				end)
				Events.on("ping", function()
					error("handler went wrong")
				end)
				Events.on("ping", function()
					Events.on("ping", function()
						counts.late += 1
					end)
				end)
				Events.emit("ping")
				gone:disconnect()
				Events.emit("ping")
				Events.emit("nobody listens")
				self:set_name(string.format("%d %d %d %d", counts.on, counts.once, counts.gone, counts.late))
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene);

		std::ostringstream log;
		Foundation::Logger::set_sink(&log);
		system.update(0.F);
		Foundation::Logger::set_sink(nullptr);
		CHECK(entity.get_component<MetadataComponent>().get_name() == "2 1 1 1");
		CHECK(log.str().find("handler went wrong") != std::string::npos);
	}
}

TEST_SUITE("Script spawning") {
	TEST_CASE("clone copies an entity with its components, children and script, next to the original") {
		std::string enemy = write_script("enemy.luau", R"(
			local M = {}
			function M.on_start(self)
				if self:is_visible() then
					self:set_name("spawned")
				end
			end
			return M
		)");
		std::string spawner = write_script("spawner.luau", R"(
			local Scene = require("@aquila").Scene
			local M = {}
			function M.on_start(self)
				local copy = Scene.find("Enemy"):clone()
				copy:set_visible(true)
				copy:set_position(vector.create(4, 0, 0))
			end
			return M
		)");
		Scene scene("Test");
		EntityManager &entities = *scene.get_entity_manager();
		Entity group = entities.create_entity("Group");
		Entity original = scripted_entity(scene, enemy);
		original.get_component<MetadataComponent>().set_name("Enemy");
		original.get_component<MetadataComponent>().set_visible(false);
		original.add_component<LightComponent>().set_range(3.F);
		entities.add_child(group, original);
		Entity eye = entities.create_entity("Eye");
		eye.get_component<TransformComponent>().set_local_position(Vec3(0.F, 1.F, 0.F));
		entities.add_child(original, eye);
		scripted_entity(scene, spawner);
		ScriptSystem system(scene);

		system.update(0.F);
		system.update(0.F);
		Option<Entity> copy;
		for (auto [handle, light] : scene.get_registry().view<LightComponent>().each()) {
			if (handle != original.get_handle()) {
				copy = Entity(handle, &scene);
			}
		}
		REQUIRE(copy.has_value());
		const auto &metadata = copy->get_component<MetadataComponent>();
		CHECK(metadata.get_name() == "spawned");
		CHECK(metadata.get_id() != original.get_component<MetadataComponent>().get_id());
		CHECK(copy->get_component<LightComponent>().get_range() == doctest::Approx(3.F));
		CHECK(copy->get_component<TransformComponent>().get_local_position() == Vec3(4.F, 0.F, 0.F));
		REQUIRE(entities.get_parent(*copy).has_value());
		CHECK(*entities.get_parent(*copy) == group);
		const auto &children = copy->get_component<SceneNodeComponent>().children;
		REQUIRE(children.size() == 1);
		CHECK(children[0] != eye);
		CHECK(children[0].get_component<MetadataComponent>().get_name() != "");
		CHECK(children[0].get_component<TransformComponent>().get_local_position() == Vec3(0.F, 1.F, 0.F));
		CHECK(original.get_component<MetadataComponent>().get_name() == "Enemy");
		CHECK(original.get_component<SceneNodeComponent>().children.size() == 1);
	}

	TEST_CASE("find_all finds every entity with a name, find_child searches below an entity") {
		std::string path = write_script("finders.luau", R"(
			local Scene = require("@aquila").Scene
			local M = {}
			function M.on_start(self)
				assert(#Scene.find_all("Coin") == 3, "three coins")
				assert(#Scene.find_all("Nope") == 0, "no nope")
				local gem = self:find_child("Gem")
				assert(gem ~= nil and gem:get_parent():get_name() == "Lid", "gem under the lid")
				assert(self:find_child("Nope") == nil, "no nope child")
				assert(self:find_child(self:get_name()) == nil, "not itself")
				self:set_name("found")
			end
			return M
		)");
		Scene scene("Test");
		EntityManager &entities = *scene.get_entity_manager();
		Entity chest = scripted_entity(scene, path);
		Entity lid = entities.create_entity("Lid");
		Entity gem = entities.create_entity("Gem");
		entities.add_child(chest, lid);
		entities.add_child(lid, gem);
		for (int i = 0; i < 3; ++i) {
			entities.create_entity("Coin").get_component<MetadataComponent>().set_name("Coin");
		}
		ScriptSystem system(scene);

		system.update(0.F);
		CHECK(chest.get_component<MetadataComponent>().get_name() == "found");
	}
}

TEST_SUITE("Aquila.Game") {
	TEST_CASE("Game.quit asks for the game to stop once the frame's scripts have run") {
		std::string path = write_script("quit.luau", R"(
			local Game = require("@aquila").Game
			local M = {}
			function M.on_update(self, dt)
				Game.quit()
				self:set_name("still finished the frame")
			end
			return M
		)");
		Scene scene("Test");
		Entity entity = scripted_entity(scene, path);
		ScriptSystem system(scene);
		CHECK_FALSE(system.wants_quit());

		system.update(0.F);
		CHECK(system.wants_quit());
		CHECK(entity.get_component<MetadataComponent>().get_name() == "still finished the frame");
	}
}

TEST_SUITE("Scene.raycast") {
	TEST_CASE("raycast finds the nearest visible mesh box along the ray") {
		std::string path = write_script("raycast.luau", R"(
			local Scene = require("@aquila").Scene
			local M = {}
			local function report(name: string, hit: Scene.RaycastHit?)
				local out = Scene.create(name)
				if hit then
					out:set_name(name .. " " .. hit.entity:get_name())
					out:set_position(hit.position)
					out:set_rotation(hit.normal)
					out:set_scale(vector.create(hit.distance, 0, 0))
				end
			end
			function M.on_start(self)
				local forward = vector.create(0, 0, 1)
				report("hit", Scene.raycast(vector.zero, forward))
				report("long", Scene.raycast(vector.zero, vector.create(0, 0, 2)))
				report("short", Scene.raycast(vector.zero, forward, 5))
				report("ignored", Scene.raycast(vector.zero, forward, nil, Scene.find("Crate")))
				report("inside", Scene.raycast(vector.create(0, 0, 10), forward))
				report("miss", Scene.raycast(vector.zero, vector.create(0, 1, 0)))
			end
			return M
		)");
		Scene scene("Test");
		EntityManager &entities = *scene.get_entity_manager();
		const auto cube =
			Graphics::Resources::MeshLibrary::instance().load(Graphics::Resources::MeshSource::procedural("cube"));
		REQUIRE(cube != nullptr);
		auto crate = [&](const char *name, Vec3 position) {
			Entity entity = entities.create_entity(name);
			entity.get_component<MetadataComponent>().set_name(name);
			entity.get_component<TransformComponent>().set_local_position(position);
			entity.add_component<MeshComponent>().set_mesh(cube);
			return entity;
		};
		Entity near = crate("Crate", Vec3(0.F, 0.F, 10.F));
		near.get_component<TransformComponent>().set_local_rotation_euler(Vec3(0.F, 30.F, 0.F));
		near.get_component<TransformComponent>().set_local_scale(Vec3(2.F));
		crate("Far", Vec3(0.F, 0.F, 20.F));
		crate("Hidden", Vec3(0.F, 0.F, 5.F)).get_component<MetadataComponent>().set_visible(false);
		scripted_entity(scene, path);
		ScriptSystem system(scene);

		system.update(0.F);
		auto result = [&](const char *name) {
			Option<Entity> found;
			for (auto [handle, metadata] : scene.get_registry().view<MetadataComponent>().each()) {
				if (metadata.get_name().starts_with(std::string(name) + " ") || metadata.get_name() == name) {
					found = Entity(handle, &scene);
				}
			}
			REQUIRE(found.has_value());
			return *found;
		};
		const F32 entry = 10.F - 1.F / std::cos(Math::radians(30.F));
		Entity hit = result("hit");
		CHECK(hit.get_component<MetadataComponent>().get_name() == "hit Crate");
		const auto &at = hit.get_component<TransformComponent>();
		check_near(at.get_local_position(), Vec3(0.F, 0.F, entry));
		check_near(at.get_local_rotation_euler(), Vec3(-0.5F, 0.F, -std::cos(Math::radians(30.F))));
		CHECK(at.get_local_scale().x == doctest::Approx(entry));
		CHECK(result("long").get_component<TransformComponent>().get_local_scale().x == doctest::Approx(entry));
		CHECK(result("short").get_component<MetadataComponent>().get_name() == "short");
		CHECK(result("ignored").get_component<MetadataComponent>().get_name() == "ignored Far");
		CHECK(result("ignored").get_component<TransformComponent>().get_local_scale().x == doctest::Approx(19.5F));
		CHECK(result("inside").get_component<MetadataComponent>().get_name() == "inside Far");
		CHECK(result("inside").get_component<TransformComponent>().get_local_scale().x == doctest::Approx(9.5F));
		CHECK(result("miss").get_component<MetadataComponent>().get_name() == "miss");
	}
}
