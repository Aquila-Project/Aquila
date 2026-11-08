#ifndef AQUILA_TESTS_UI_RENDER_TEST_SUPPORT_H
#define AQUILA_TESTS_UI_RENDER_TEST_SUPPORT_H

#include <doctest.h>

#include "stb/stb_image_write.h"

#include "Aquila/GFX/GfxBuffer.h"
#include "Aquila/GFX/GfxRenderpass.h"
#include "Aquila/Graphics/Core/Renderer2D.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/FontRegistry.h"
#include "GpuTestSupport.h"

struct RenderFonts {
	Unique<Aquila::UI::Text::FontAtlas> regular;
	Unique<Aquila::UI::Text::FontAtlas> mono;

	explicit RenderFonts(Aquila::GFX::GfxContext &ctx)
		: regular(
			  Aquila::UI::Text::FontAtlas::create_from_file(ctx, "/resources/Engine/Fonts/Geist/Geist-Regular.ttf")),
		  mono(Aquila::UI::Text::FontAtlas::create_from_file(
			  ctx, "/resources/Engine/Fonts/Inconsolata/Inconsolata-Regular.ttf")) {
		REQUIRE(regular != nullptr);
		REQUIRE(mono != nullptr);
		for (const char *family : { "regular", "medium", "bold" }) {
			Aquila::UI::Core::FontRegistry::Register(family, regular.get());
		}
		Aquila::UI::Core::FontRegistry::Register("mono", mono.get());
	}

	~RenderFonts() {
		for (const char *family : { "regular", "medium", "mono", "bold" }) {
			Aquila::UI::Core::FontRegistry::Register(family, test_gpu().font.get());
		}
	}
	AQUILA_NONCOPYABLE(RenderFonts);
	AQUILA_NONMOVEABLE(RenderFonts);
};

inline void write_png(Aquila::GFX::GfxContext &ctx, Aquila::UI::Core::Canvas &canvas, Uint32 width, Uint32 height,
					  const std::string &path) {
	Aquila::RHI::TextureDesc desc{};
	desc.width = width;
	desc.height = height;
	desc.format = Aquila::RHI::TextureFormat::RGBA8;
	desc.usage = Aquila::RHI::TextureUsage::ColorAttachment | Aquila::RHI::TextureUsage::TransferSrc;
	desc.debug_name = "RenderTest";
	Ref<Aquila::GFX::GfxTexture> target = ctx.create_texture(desc);
	REQUIRE(target != nullptr);

	Aquila::RHI::RenderPassDesc pass_desc{};
	pass_desc.color_attachments = { {
		.texture = &target->get_rhi(),
		.clear_color = Vec4(0.122F, 0.122F, 0.122F, 1.F),
		.load_op = Aquila::RHI::AttachmentLoadOp::Clear,
		.store_op = Aquila::RHI::AttachmentStoreOp::Store,
	} };
	pass_desc.width = width;
	pass_desc.height = height;
	pass_desc.debug_name = "RenderTest";
	pass_desc.external_barriers = true;
	Ref<Aquila::GFX::GfxRenderPass> pass = ctx.create_render_pass(pass_desc);
	REQUIRE(pass != nullptr);

	Aquila::RHI::BufferDesc buffer_desc{};
	buffer_desc.size = static_cast<Uint64>(width) * height * 4;
	buffer_desc.usage = Aquila::RHI::BufferUsage::TransferDst;
	buffer_desc.domain = Aquila::RHI::MemoryDomain::GpuToCpu;
	buffer_desc.debug_name = "RenderTestReadback";
	Ref<Aquila::GFX::GfxBuffer> readback = ctx.create_buffer(buffer_desc);
	REQUIRE(readback != nullptr);

	Aquila::Graphics::Renderer2D batcher(ctx);
	const Mat4 ortho = glm::ortho(0.F, static_cast<F32>(width), static_cast<F32>(height), 0.F, -1.F, 1.F);
	ctx.execute_immediate(Aquila::RHI::CommandListType::Graphics, [&](Aquila::GFX::GfxCommandList &cmd) {
		cmd.transition_texture(*target, Aquila::RHI::ResourceState::Undefined,
							   Aquila::RHI::ResourceState::ColorAttachmentWrite);
		pass->begin(cmd);
		cmd.set_viewport(0.F, 0.F, static_cast<F32>(width), static_cast<F32>(height));
		cmd.set_scissor(0, 0, width, height);
		batcher.begin(cmd, Aquila::RHI::TextureFormat::RGBA8, Aquila::RHI::SampleCount::X1, ortho);
		canvas.submit_to_quad_batcher(batcher, cmd);
		batcher.end();
		pass->end(cmd);
		cmd.transition_texture(*target, Aquila::RHI::ResourceState::ColorAttachmentWrite,
							   Aquila::RHI::ResourceState::TransferSrc);
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

#endif
