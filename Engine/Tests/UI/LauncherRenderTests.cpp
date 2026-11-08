#include <doctest.h>

#include "Aquila/Graphics/Shader/ShaderHotReload.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"
#include "Aquila/UI/Core/TextureCache.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "LoaderTestSupport.h"
#include "RenderTestSupport.h"

using namespace Aquila;
using namespace Aquila::UI::Core;

namespace {

constexpr F32 K_SCALE = 2.F;
constexpr Uint32 K_WIDTH = 900;
constexpr Uint32 K_HEIGHT = 600;
constexpr const char *K_LAYOUT = "/resources/Engine/UI/Windows/launcher.aqlayout";

struct RecentRow {
	const char *name;
	const char *path;
	const char *when;
};

constexpr std::array<RecentRow, 4> K_ROWS = { {
	{ "Sponza", "/home/user/Aquila/Projects/Sponza", "2 hours ago" },
	{ "Lighting Tests", "/home/user/Aquila/Projects/Lighting_Tests", "yesterday" },
	{ "Shader Playground", "/mnt/data/work/rendering/experiments/2026/shader-playground", "3 days ago" },
	{ "Terrain", "/home/user/Aquila/Projects/Terrain", "2 months ago" },
} };

void render_launcher(GFX::GfxContext &ctx, TextureCache &textures, bool new_page, bool invalid_name,
					 const std::string &path) {
	const auto width = static_cast<Uint32>(static_cast<F32>(K_WIDTH) * K_SCALE);
	const auto height = static_cast<Uint32>(static_cast<F32>(K_HEIGHT) * K_SCALE);
	LoaderCanvas fixture(width, height);
	fixture.loader.register_texture_cache(&textures);

	Unique<View> layout = fixture.loader.load_file(K_LAYOUT);
	REQUIRE(layout != nullptr);
	View *root = fixture.host().add_child(std::move(layout));
	root->find_by_id<Label>("launcher-version")->set_text("Engine 0.2.5");
	root->find_by_id("launcher-empty")->set_class("hidden", true);
	for (const RecentRow &row : K_ROWS) {
		REQUIRE(fixture.loader.create(
					K_LAYOUT, "LauncherRecent", *root->find_by_id("launcher-recent"),
					{ .attributes = { { "name", row.name }, { "path", row.path }, { "when", row.when } } }) != nullptr);
	}

	if (new_page) {
		root->find_by_id("launcher-page-home")->set_class("hidden", true);
		root->find_by_id("launcher-page-new")->set_class("hidden", false);
		root->find_by_id<TextInput>("launcher-name")->set_text("New Project");
		root->find_by_id<TextInput>("launcher-location")->set_text("/home/user/Aquila/Projects");
		root->find_by_id<Label>("launcher-preview")->set_text("/home/user/Aquila/Projects/New_Project");
	}
	if (invalid_name) {
		root->find_by_id("launcher-page-home")->set_class("hidden", true);
		root->find_by_id("launcher-page-new")->set_class("hidden", false);
		root->find_by_id<TextInput>("launcher-location")->set_text("/home/user/Aquila/Projects");
		root->find_by_id("launcher-name")->set_class("launcher-input-invalid", true);
		auto *error = root->find_by_id<Label>("launcher-name-error");
		error->set_text("Enter a project name.");
		error->set_class("hidden", false);
		root->find_by_id<Label>("launcher-preview")->set_text("/home/user/Aquila/Projects/...");
		root->find_by_id("launcher-create")->set_class("launcher-create-disabled", true);
	}

	fixture.canvas->reload_styles();
	fixture.settle_fully();
	write_png(ctx, *fixture.canvas, width, height, path);
}

}

TEST_SUITE("Launcher render") {
	TEST_CASE("render the launcher to a png" * doctest::skip()) {
		TestGpu &gpu = test_gpu();
		GFX::GfxContext &ctx = *gpu.ctx;
		RHI::VulkanShaderCompiler::initialize();
		Graphics::Shader::ShaderHotReload::init();
		{
			RenderFonts fonts(ctx);
			FontRegistry::set_ui_scale(K_SCALE);
			TextureCache textures(ctx, "/resources");
			render_launcher(ctx, textures, false, false, "UIRenders/launcher.png");
			render_launcher(ctx, textures, true, false, "UIRenders/launcher_new.png");
			render_launcher(ctx, textures, false, true, "UIRenders/launcher_new_invalid.png");
		}
		FontRegistry::set_ui_scale(1.F);
		Graphics::Shader::ShaderHotReload::shutdown();
	}
}
