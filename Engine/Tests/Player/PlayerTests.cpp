#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "Aquila/Player/BuildLayout.h"
#include "Aquila/Player/BuildManifest.h"
#include "Aquila/Player/BuildPlan.h"
#include "Aquila/Player/BuildRunner.h"
#include "Aquila/Graphics/Shader/ShaderProgram.h"
#include "Aquila/RHI/ShaderPack.h"

using namespace Aquila::Player;
namespace fs = std::filesystem;

namespace {

BuildRequest valid_request() {
	return {
		.project_name = "My Game",
		.project_dir = "/projects/MyGame",
		.startup_scene = "MyGame.aqscene",
		.template_dir = "/editor/Templates/Player",
		.output_dir = "/builds",
		.window_width = 1600,
		.window_height = 900,
		.icon_path = {},
	};
}

std::vector<BuildStepKind> kinds(const BuildPlan &plan) {
	std::vector<BuildStepKind> out;
	for (const BuildStep &step : plan.steps) {
		out.push_back(step.kind);
	}
	return out;
}

void write(const fs::path &path, std::string_view text) {
	fs::create_directories(path.parent_path());
	std::ofstream(path) << text;
}

std::string read(const fs::path &path) {
	std::ifstream file(path);
	return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
}

struct Sandbox {
	fs::path root = fs::temp_directory_path() / "aquila_player_tests";

	Sandbox() {
		fs::remove_all(root);
		write(root / "template" / AQUILA_PLAYER_EXECUTABLE_NAME, "player");
		write(root / "template" / AQUILA_PLAYER_ENGINE_SHADERS_DIR / "Basic.slang", "shader");
		write(root / "project" / "project.aqproject", "{}");
		write(root / "project" / "Main.aqscene", "{}");
		write(root / "project" / "Content" / "notes.txt", "content");
		write(root / "project" / "Content" / "Icon.PNG", "png");
	}

	~Sandbox() { fs::remove_all(root); }

	[[nodiscard]] BuildRequest request(std::string name) const {
		return {
			.project_name = std::move(name),
			.project_dir = (root / "project").string(),
			.startup_scene = "Main.aqscene",
			.template_dir = (root / "template").string(),
			.output_dir = (root / "builds").string(),
			.window_width = 1280,
			.window_height = 720,
			.icon_path = {},
		};
	}
};

}

TEST_SUITE("Player build") {
	TEST_CASE("the layout keeps everything under Data") {
		const BuildLayout layout{ .root = "/out/Game" };
		const fs::path root = "/out/Game";
		CHECK(fs::path(layout.manifest_file()) == root / AQUILA_PLAYER_MANIFEST);
		CHECK(fs::path(layout.engine_resources_dir()) == root / AQUILA_PLAYER_ENGINE_RESOURCES_DIR);
		CHECK(fs::path(layout.engine_shaders_dir()) == root / AQUILA_PLAYER_ENGINE_SHADERS_DIR);
		CHECK(fs::path(layout.project_dir()) == root / AQUILA_PLAYER_PROJECT_DIR);
		CHECK(fs::path(layout.icon_file()) == root / AQUILA_PLAYER_ICON);
		CHECK(fs::path(layout.executable("Game")) == root / (std::string("Game") + AQUILA_PLAYER_EXECUTABLE_SUFFIX));
		CHECK(fs::path(player_template_dir("/editor")) == fs::path("/editor") / AQUILA_PLAYER_TEMPLATE_DIR);
	}

	TEST_CASE("a manifest survives a round trip") {
		const BuildManifest manifest{ .version = 1,
									  .name = "My Game",
									  .startup_scene = "Main.aqscene",
									  .window_width = 1920,
									  .window_height = 1080 };
		const Option<BuildManifest> back = read_manifest(write_manifest(manifest));
		REQUIRE(back.has_value());
		CHECK(back->name == "My Game");
		CHECK(back->startup_scene == "Main.aqscene");
		CHECK(back->window_width == 1920);
		CHECK(back->window_height == 1080);
	}

	TEST_CASE("the project's player settings reach the manifest") {
		BuildRequest request = valid_request();
		request.window_title = "Pixel Garden";
		request.maximized = true;
		request.tone_mapper = Aquila::Rendering::ToneMapper::Neutral;
		request.shadows = false;
		const BuildPlan plan = plan_build(request);
		REQUIRE(plan.ok());

		const Option<BuildManifest> back = read_manifest(plan.steps.back().content);
		REQUIRE(back.has_value());
		CHECK(back->window_title == "Pixel Garden");
		CHECK(back->maximized);
		CHECK(back->tone_mapper == Aquila::Rendering::ToneMapper::Neutral);
		CHECK_FALSE(back->shadows);

		const Option<BuildManifest> old =
			read_manifest(R"({"version": 1, "name": "x", "startup_scene": "a.aqscene"})");
		REQUIRE(old.has_value());
		CHECK(old->window_title.empty());
		CHECK_FALSE(old->maximized);
		CHECK(old->tone_mapper == Aquila::Rendering::ToneMapper::AgX);
		CHECK(old->shadows);
	}

	TEST_CASE("broken manifests are rejected and missing sizes use defaults") {
		CHECK_FALSE(read_manifest("not json").has_value());
		CHECK_FALSE(read_manifest("{}").has_value());
		CHECK_FALSE(read_manifest(R"({"version": 999, "name": "x", "startup_scene": "a.aqscene"})").has_value());

		const Option<BuildManifest> minimal =
			read_manifest(R"({"version": 1, "name": "x", "startup_scene": "a.aqscene"})");
		REQUIRE(minimal.has_value());
		CHECK(minimal->window_width == BuildManifest{}.window_width);
	}

	TEST_CASE("a valid request plans every step in order") {
		const BuildPlan plan = plan_build(valid_request());
		REQUIRE(plan.ok());
		CHECK(fs::path(plan.layout.root) == fs::path("/builds") / "My_Game");
		CHECK(plan.manifest.name == "My Game");
		CHECK(plan.manifest.window_width == 1600);
		REQUIRE(kinds(plan) ==
				std::vector{ BuildStepKind::MakeDirectory, BuildStepKind::CopyDirectory, BuildStepKind::Rename,
							 BuildStepKind::CopyDirectory, BuildStepKind::BakeShaders, BuildStepKind::WriteFile });

		const BuildStep &rename = plan.steps[2];
		CHECK(fs::path(rename.source) == fs::path(plan.layout.root) / AQUILA_PLAYER_EXECUTABLE_NAME);
		CHECK(rename.target == plan.layout.executable("My_Game"));

		const BuildStep &bake = plan.steps[4];
		CHECK(bake.source == "/projects/MyGame");
		CHECK(bake.target == plan.layout.shader_pack_file());

		const BuildStep &manifest = plan.steps[5];
		CHECK(manifest.target == plan.layout.manifest_file());
		CHECK(read_manifest(manifest.content).has_value());
	}

	TEST_CASE("invalid requests are reported without steps") {
		std::vector<BuildRequest> requests(6, valid_request());
		requests[0].project_name.clear();
		requests[1].project_dir.clear();
		requests[2].template_dir.clear();
		requests[3].output_dir.clear();
		requests[4].startup_scene = "Main.txt";
		requests[5].output_dir = "/projects/MyGame/Builds";
		for (const BuildRequest &request : requests) {
			const BuildPlan plan = plan_build(request);
			CHECK_FALSE(plan.ok());
			CHECK(plan.steps.empty());
		}
	}

	TEST_CASE("a build produces a self-contained folder") {
		const Sandbox sandbox;
		const BuildPlan plan = plan_build(sandbox.request("My Game"));
		REQUIRE(plan.ok());

		Usize progress_calls = 0;
		const BuildResult result = run_build(plan, [&progress_calls](const BuildProgress &) { ++progress_calls; });
		REQUIRE_MESSAGE(result.success, (result.errors.empty() ? std::string() : result.errors.front()));
		CHECK(progress_calls == plan.steps.size());

		const fs::path root = plan.layout.root;
		CHECK(fs::exists(result.executable));
		CHECK_FALSE(fs::exists(root / AQUILA_PLAYER_EXECUTABLE_NAME));
		CHECK(read(root / AQUILA_PLAYER_ENGINE_SHADERS_DIR / "Basic.slang") == "shader");
		CHECK(read(root / AQUILA_PLAYER_PROJECT_DIR / "Main.aqscene") == "{}");
		CHECK(read(root / AQUILA_PLAYER_PROJECT_DIR / "Content" / "notes.txt") == "content");
		CHECK(read_manifest(read(plan.layout.manifest_file())).has_value());
#ifndef _WIN32
		CHECK((fs::status(result.executable).permissions() & fs::perms::owner_exec) != fs::perms::none);
#endif
	}

	TEST_CASE("a build compiles the project's shaders and ships no shader source") {
		const Sandbox sandbox;
		write(sandbox.root / "template" / AQUILA_PLAYER_ENGINE_RESOURCES_DIR / "Engine" / "Templates" / "lit.slang",
			  "import Aquila.Lit;");
		write(sandbox.root / "project" / "Content" / "Shaders" / "Tint.slang",
			  "float3 tint(float3 color) { return color * 0.5; }\n");
		write(sandbox.root / "project" / "Content" / "Shaders" / "Water.slang", R"(import Aquila.Lit;
import Tint;

[[vk::binding(0, 1)]]
cbuffer Material {
	float4 baseColor;
	float roughness;
};

[[vk::binding(1, 1)]] Sampler2D foamMap;

void surface(inout Surface s, SurfaceInput input) {
	s.baseColor = tint(baseColor.rgb) * foamMap.Sample(input.uv).rgb;
	s.roughness = roughness;
}
)");

		const BuildPlan plan = plan_build(sandbox.request("Shaded"));
		REQUIRE(plan.ok());
		const BuildResult result = run_build(plan);
		REQUIRE_MESSAGE(result.success, (result.errors.empty() ? std::string() : result.errors.front()));

		Aquila::RHI::ShaderPack pack;
		std::string error;
		REQUIRE_MESSAGE(pack.load(plan.layout.shader_pack_file(), error), error);
		CHECK(pack.size() == 3);
		const Aquila::RHI::ShaderPackEntry *water = pack.find("/project/Content/Shaders/Water.slang");
		REQUIRE(water != nullptr);
		CHECK(water->stages.size() == 2);
		CHECK(pack.find("/project/Content/Shaders/Water.slang:depth") != nullptr);
		CHECK(pack.find("/project/Content/Shaders/Water.slang:emissive") != nullptr);

		using Aquila::Graphics::Shader::ShaderProgram;
		Aquila::Graphics::Shader::ShaderDomain domain{};
		std::vector<ShaderProgram::ReflectedBinding> bindings;
		REQUIRE(ShaderProgram::read_metadata(water->metadata, domain, bindings));
		CHECK(domain == Aquila::Graphics::Shader::ShaderDomain::Lit);
		const auto material = std::ranges::find_if(bindings, [](const auto &b) {
			return b.set == 1 && b.type == ShaderProgram::ReflectedBindingType::UniformBuffer;
		});
		REQUIRE(material != bindings.end());
		REQUIRE(material->ubo_fields.size() == 2);
		CHECK(material->ubo_fields[1].name == "roughness");
		CHECK(material->ubo_fields[1].offset == 16);
		CHECK(std::ranges::any_of(bindings, [](const auto &b) { return b.name == "foamMap" && b.binding_index == 1; }));

		for (const fs::path folder :
			 { fs::path(plan.layout.project_dir()), fs::path(plan.layout.engine_resources_dir()) }) {
			for (const auto &entry : fs::recursive_directory_iterator(folder)) {
				CHECK_MESSAGE(entry.path().extension() != ".slang", entry.path().string());
			}
		}
		CHECK(read(sandbox.root / "project" / "Content" / "Shaders" / "Tint.slang").starts_with("float3"));
	}

	TEST_CASE("a shader that doesn't compile fails the build") {
		const Sandbox sandbox;
		write(sandbox.root / "project" / "Content" / "Broken.slang", "import Aquila.Lit;\nvoid surface(oops) {}\n");
		const BuildResult result = run_build(plan_build(sandbox.request("Broken")));
		CHECK_FALSE(result.success);
		REQUIRE_FALSE(result.errors.empty());
		CHECK(result.errors.front().find("/project/Content/Broken.slang") != std::string::npos);
	}

	TEST_CASE("a rebuild replaces an earlier build but never a foreign folder") {
		const Sandbox sandbox;
		const BuildPlan plan = plan_build(sandbox.request("Game"));
		REQUIRE(run_build(plan).success);
		CHECK(run_build(plan).success);

		write(sandbox.root / "builds" / "Precious" / "keep.txt", "mine");
		const BuildResult foreign = run_build(plan_build(sandbox.request("Precious")));
		CHECK_FALSE(foreign.success);
		CHECK(read(sandbox.root / "builds" / "Precious" / "keep.txt") == "mine");
	}

	TEST_CASE("broken manifests never throw") {
		CHECK_FALSE(read_manifest(R"({"version": 1, "name": 5, "startup_scene": "a.aqscene"})").has_value());
		CHECK_FALSE(read_manifest(R"({"version": "one", "name": "x", "startup_scene": "a.aqscene"})").has_value());
		CHECK_FALSE(read_manifest("[]").has_value());
		CHECK_FALSE(load_manifest("/definitely/not/here.aqbuild").has_value());
	}

	TEST_CASE("a picked icon is copied into the build") {
		const Sandbox sandbox;
		BuildRequest request = sandbox.request("Iconic");
		request.icon_path = (sandbox.root / "project" / "Content" / "Icon.PNG").string();
		const BuildPlan plan = plan_build(request);
		REQUIRE(plan.ok());
		REQUIRE(plan.steps.back().kind == BuildStepKind::CopyFile);
		CHECK(plan.steps.back().target == plan.layout.icon_file());

		REQUIRE(run_build(plan).success);
		CHECK(read(plan.layout.icon_file()) == "png");
	}

	TEST_CASE("an icon that is not an existing png is reported without steps") {
		const Sandbox sandbox;
		for (const fs::path &icon : { sandbox.root / "project" / "Content" / "notes.txt",
									  sandbox.root / "project" / "Content" / "missing.png" }) {
			BuildRequest request = sandbox.request("Iconic");
			request.icon_path = icon.string();
			const BuildPlan plan = plan_build(request);
			CHECK_FALSE(plan.ok());
			CHECK(plan.steps.empty());
		}
	}
}
