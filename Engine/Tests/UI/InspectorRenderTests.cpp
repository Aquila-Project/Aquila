#include <doctest.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include "Aquila/GFX/GfxBuffer.h"
#include "Aquila/GFX/GfxRenderpass.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"
#include "Aquila/Graphics/Core/Renderer2D.h"
#include "Aquila/Graphics/Shader/ShaderHotReload.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/FontRegistry.h"
#include "Aquila/UI/Core/TextureCache.h"
#include "Aquila/UI/Widgets/AssetSlot.h"
#include "GpuTestSupport.h"
#include "InspectorFixture.h"
#include "LoaderTestSupport.h"
#include "StyleSnapshot.h"

using namespace Aquila;
using namespace Aquila::UI;
using namespace Aquila::UI::Core;

namespace {

constexpr F32 K_SCALE = 2.F;
constexpr Uint32 K_WIDTH = 340;
constexpr Uint32 K_HEIGHT = 1500;

enum class ShaderKind { Lit, Unlit, Custom };
enum class Blend { Opaque, Masked, Transparent };

struct MaterialLike {
	ShaderKind shader = ShaderKind::Lit;
	bool double_sided = false;
	Blend blend = Blend::Masked;
	F32 alpha_cutoff = 0.5F;
	Vec4 base_color = Vec4(0.8F, 0.35F, 0.2F, 1.F);
	F32 metallic = 0.F;
	F32 roughness = 0.5F;
	F32 normal_strength = 1.F;
	Int32 layers = 16;
	Vec2 uv_scale = Vec2(1.F);
	Vec2 uv_offset = Vec2(0.F);
	std::string base_color_map = "/project/Content/Textures/Tiles_GreenAndGoldPattern_01_basecolor.jpg";
	std::string normal_map;
};

const Reflection::TypeInfo &material_type() {
	using Reflection::TypeBuilder;
	using Reflection::TypeInfo;
	static const TypeInfo type = [] {
		TypeInfo info("MaterialLike");
		TypeBuilder<MaterialLike>(info)
			.property("Shader", &MaterialLike::shader)
			.options(
				{ { .name = "Lit", .value = 0 }, { .name = "Unlit", .value = 1 }, { .name = "Custom", .value = 2 } })
			.property("Double Sided", &MaterialLike::double_sided)
			.property("Blend Mode", &MaterialLike::blend)
			.options({ { .name = "Opaque", .value = 0 },
					   { .name = "Masked", .value = 1 },
					   { .name = "Transparent", .value = 2 } })
			.property("Alpha Cutoff", &MaterialLike::alpha_cutoff,
					  { .min = 0.F, .max = 1.F, .speed = 0.01F, .precision = 2, .slider = true })
			.property("Base Color", &MaterialLike::base_color, { .speed = 0.01F, .precision = 3, .color = true })
			.property("Metallic", &MaterialLike::metallic,
					  { .min = 0.F, .max = 1.F, .speed = 0.01F, .precision = 3, .slider = true })
			.property("Roughness", &MaterialLike::roughness,
					  { .min = 0.F, .max = 1.F, .speed = 0.01F, .precision = 3, .slider = true })
			.property("Normal Strength", &MaterialLike::normal_strength, { .speed = 0.01F, .precision = 3 })
			.property("Layers", &MaterialLike::layers, { .speed = 1.F })
			.property("UV Scale", &MaterialLike::uv_scale, { .speed = 0.01F, .precision = 3 })
			.property("UV Offset", &MaterialLike::uv_offset, { .speed = 0.01F, .precision = 3 })
			.property("Base Color Map", &MaterialLike::base_color_map, { .asset_type = "texture" })
			.property("Normal Map", &MaterialLike::normal_map, { .asset_type = "texture" });
		return info;
	}();
	return type;
}

struct Fonts {
	Unique<Text::FontAtlas> regular;
	Unique<Text::FontAtlas> mono;

	explicit Fonts(GFX::GfxContext &ctx)
		: regular(Text::FontAtlas::create_from_file(ctx, "/resources/Engine/Fonts/Geist/Geist-Regular.ttf")),
		  mono(Text::FontAtlas::create_from_file(ctx, "/resources/Engine/Fonts/Inconsolata/Inconsolata-Regular.ttf")) {
		REQUIRE(regular != nullptr);
		REQUIRE(mono != nullptr);
		for (const char *family : { "regular", "medium", "bold" }) {
			FontRegistry::Register(family, regular.get());
		}
		FontRegistry::Register("mono", mono.get());
	}

	~Fonts() {
		for (const char *family : { "regular", "medium", "mono", "bold" }) {
			FontRegistry::Register(family, test_gpu().font.get());
		}
	}
	AQUILA_NONCOPYABLE(Fonts);
	AQUILA_NONMOVEABLE(Fonts);
};

struct Section {
	PropertyGrid *grid = nullptr;
	Unique<ReflectedPropertyGrid> reflected;
};

Section add_section(LayoutLoader &loader, InspectorPage &page, const std::string &title) {
	const std::string components = engine_component_file("inspector");
	auto *section = dynamic_cast<Collapsible *>(
		loader.create(components, "InspectorPanel", page.get_sections(), { .attributes = { { "title", title } } }));
	REQUIRE(section != nullptr);
	section->add_header_action(nullptr, "Remove", [] {});
	Section result;
	result.grid = section->add_child<PropertyGrid>();
	result.grid->set_split(true);
	return result;
}

void write_png(GFX::GfxContext &ctx, Canvas &canvas, Uint32 width, Uint32 height, const std::string &path) {
	RHI::TextureDesc desc{};
	desc.width = width;
	desc.height = height;
	desc.format = RHI::TextureFormat::RGBA8;
	desc.usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::TransferSrc;
	desc.debug_name = "InspectorRender";
	Ref<GFX::GfxTexture> target = ctx.create_texture(desc);
	REQUIRE(target != nullptr);

	RHI::RenderPassDesc pass_desc{};
	pass_desc.color_attachments = { {
		.texture = &target->get_rhi(),
		.clear_color = Vec4(0.122F, 0.122F, 0.122F, 1.F),
		.load_op = RHI::AttachmentLoadOp::Clear,
		.store_op = RHI::AttachmentStoreOp::Store,
	} };
	pass_desc.width = width;
	pass_desc.height = height;
	pass_desc.debug_name = "InspectorRender";
	pass_desc.external_barriers = true;
	Ref<GFX::GfxRenderPass> pass = ctx.create_render_pass(pass_desc);
	REQUIRE(pass != nullptr);

	RHI::BufferDesc buffer_desc{};
	buffer_desc.size = static_cast<Uint64>(width) * height * 4;
	buffer_desc.usage = RHI::BufferUsage::TransferDst;
	buffer_desc.domain = RHI::MemoryDomain::GpuToCpu;
	buffer_desc.debug_name = "InspectorRenderReadback";
	Ref<GFX::GfxBuffer> readback = ctx.create_buffer(buffer_desc);
	REQUIRE(readback != nullptr);

	Graphics::Renderer2D batcher(ctx);
	const Mat4 ortho = glm::ortho(0.F, static_cast<F32>(width), static_cast<F32>(height), 0.F, -1.F, 1.F);
	ctx.execute_immediate(RHI::CommandListType::Graphics, [&](GFX::GfxCommandList &cmd) {
		cmd.transition_texture(*target, RHI::ResourceState::Undefined, RHI::ResourceState::ColorAttachmentWrite);
		pass->begin(cmd);
		cmd.set_viewport(0.F, 0.F, static_cast<F32>(width), static_cast<F32>(height));
		cmd.set_scissor(0, 0, width, height);
		batcher.begin(cmd, RHI::TextureFormat::RGBA8, RHI::SampleCount::X1, ortho);
		canvas.submit_to_quad_batcher(batcher, cmd);
		batcher.end();
		pass->end(cmd);
		cmd.transition_texture(*target, RHI::ResourceState::ColorAttachmentWrite, RHI::ResourceState::TransferSrc);
		cmd.copy_texture_to_buffer(*target, *readback, width, height);
	});
	ctx.wait_idle();

	std::filesystem::create_directories(std::filesystem::path(path).parent_path());
	const auto *mapped = static_cast<const Uint8 *>(readback->map());
	REQUIRE(mapped != nullptr);
	std::vector<Uint8> pixels(mapped, mapped + buffer_desc.size);
	readback->unmap();
	for (Usize i = 3; i < pixels.size(); i += 4) {
		pixels[i] = 255;
	}
	CHECK(stbi_write_png(path.c_str(), static_cast<int>(width), static_cast<int>(height), 4, pixels.data(),
						 static_cast<int>(width * 4)) != 0);
	MESSAGE("wrote " << std::filesystem::absolute(path).string());
}

}

TEST_SUITE("Inspector render") {
	TEST_CASE("render the inspector to a png" * doctest::skip()) {
		TestGpu &gpu = test_gpu();
		GFX::GfxContext &ctx = *gpu.ctx;
		RHI::VulkanShaderCompiler::initialize();
		Graphics::Shader::ShaderHotReload::init();
		Fonts fonts(ctx);
		FontRegistry::set_ui_scale(K_SCALE);
		const auto width = static_cast<Uint32>(static_cast<F32>(K_WIDTH) * K_SCALE);
		const auto height = static_cast<Uint32>(static_cast<F32>(K_HEIGHT) * K_SCALE);

		TextureCache textures(ctx);
		GFX::GfxTexture *thumbnail = textures.load("/resources/Engine/aquila-logo.png");
		REQUIRE(thumbnail != nullptr);
		AssetSlot::set_adapter("texture", { .thumbnail = [thumbnail](const std::string &) { return thumbnail; } });

		{
			LoaderCanvas fixture(width, height);
			View *box = fixture.host().add_child<View>();
			StyleProperties style;
			style.width = StyleLength::percent(100.F);
			style.height = StyleLength::percent(100.F);
			style.flex_direction = FlexDirection::Column;
			style.background_color = Vec4(0.122F, 0.122F, 0.122F, 1.F);
			box->merge_style(style);

			Unique<View> layout = fixture.loader.load_file("/resources/Engine/UI/Windows/inspector.aqlayout");
			REQUIRE(layout != nullptr);
			View *root = box->add_child(std::move(layout));
			const std::string components = engine_component_file("inspector");
			for (int i = 0; i < 3; ++i) {
				REQUIRE(fixture.loader.create(components, "InspectorTabButton", *root->find_by_id("props-tabs")) !=
						nullptr);
			}
			root->find_by_id("inspector-empty")->set_hidden(true);
			root->find_by_id<TextInput>("inspector-name")->set_text("Floor Tiles");
			root->find_by_id<Label>("inspector-uuid")->set_text("550e8400-e29b-41d4-a716-446655440000");
			auto *page =
				fixture.loader.create<InspectorPage>(components, "InspectorPage", *root->find_by_id("inspector-pages"));
			REQUIRE(page != nullptr);

			MaterialLike material;
			Section material_section = add_section(fixture.loader, *page, "Material");
			material_section.grid->add_row<Label>("Editing", std::string("This entity's own material"));
			material_section.grid->add_row<Button>("", std::string("Save as Material Asset"));
			material_section.reflected =
				std::make_unique<ReflectedPropertyGrid>(*material_section.grid, material_type(), &ctx);
			material_section.reflected->bind(&material);

			InspectorFixture::Everything everything;
			Section everything_section = add_section(fixture.loader, *page, "Every Widget");
			everything_section.reflected = std::make_unique<ReflectedPropertyGrid>(
				*everything_section.grid, InspectorFixture::everything_type(), &ctx);
			everything_section.reflected->bind(&everything);

			InspectorFixture::EverythingInspector::style_dropdowns(page->root());
			auto *add = dynamic_cast<Button *>(fixture.loader.create(components, "InspectorAction", page->root()));
			REQUIRE(add != nullptr);
			add->set_text("Add Component");

			fixture.canvas->reload_styles();
			fixture.settle_fully();
			write_png(ctx, *fixture.canvas, width, height, "UIRenders/inspector.png");
			std::ofstream("UIRenders/inspector.txt", std::ios::trunc) << style_snapshot(*box);
		}

		AssetSlot::set_adapter("texture", {});
		FontRegistry::set_ui_scale(1.F);
		Graphics::Shader::ShaderHotReload::shutdown();
	}
}
