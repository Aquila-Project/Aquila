#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "Aquila/Platform/Filesystem/NativeFileSystem.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Scene/ComponentRegistry.h"
#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scripting/ScriptComponent.h"
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
}
