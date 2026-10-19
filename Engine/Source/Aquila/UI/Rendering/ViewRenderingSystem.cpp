#include "Aquila/UI/Rendering/ViewRenderingSystem.h"
#include "Aquila/UI/Core/CanvasManager.h"
#include "Aquila/Graphics/Core/Renderer2D.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"
#include "Aquila/Graphics/RenderGraph/RGTypes.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/GFX/GfxSwapchain.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/Profiler.h"

namespace Aquila::UI::Rendering {

void ViewRenderingSystem::on_init(GFX::GfxContext &ctx) {
	m_ctx = &ctx;
	m_r2_d = std::make_unique<Graphics::Renderer2D>(ctx);
	m_overlay_r2d = std::make_unique<Graphics::Renderer2D>(ctx);
}

void ViewRenderingSystem::rebuild_overlay_resources(Uint32 w, Uint32 h) {
	m_msaa_color = m_ctx->create_texture({
		.width = w,
		.height = h,
		.format = RHI::TextureFormat::BGRA8,
		.usage = RHI::TextureUsage::ColorAttachment,
		.samples = RHI::SampleCount::X4,
		.debug_name = "UIMSAAColor",
	});

	m_overlay_pass = m_ctx->create_render_pass({
		.color_attachments = { {
			.texture = &m_msaa_color->get_rhi(),
			.load_op = RHI::AttachmentLoadOp::Clear,
			.store_op = RHI::AttachmentStoreOp::DontCare,
		} },
		.use_swapchain_as_resolve = true,
		.debug_name = "UIOverlay",
	});
}

void ViewRenderingSystem::add_passes(Graphics::RG::RenderGraph &graph, Aquila::Rendering::FrameContext &ctx) {
	const Uint32 w = ctx.width;
	const Uint32 h = ctx.height;
	auto *r2d = m_r2_d.get();

	// WorldSpace + ScreenCamera composite into scene color.
	graph.add_pass(
		"GameUI",
		[&ctx](Graphics::RG::RGPassBuilder &builder) {
			ctx.h_display_color = builder.set_color_attachment(
				0, ctx.h_display_color, Graphics::RG::AttachmentLoadOp::Load, Graphics::RG::AttachmentStoreOp::Store);
		},
		[r2d, w, h](GFX::GfxCommandList &cmd, Graphics::RG::RGRegistry &) {
			const Mat4 ortho = glm::ortho(0.F, static_cast<float>(w), static_cast<float>(h), 0.F, -1.F, 1.F);
			r2d->begin(cmd, Aquila::Rendering::DISPLAY_COLOR_FORMAT, RHI::SampleCount::X1, ortho);
			Core::CanvasManager::get()->render_layers(*r2d, cmd, Core::UILayer::WorldSpace,
													  Core::UILayer::ScreenCamera);
			r2d->end();
		});
}

void ViewRenderingSystem::blit_to_swapchain(Graphics::RG::RenderGraph &graph, Aquila::Rendering::FrameContext &ctx) {
	if (ctx.swapchain == nullptr) {
		return;
	}

	const Uint32 w = ctx.swapchain->get_width();
	const Uint32 h = ctx.swapchain->get_height();

	if (!m_overlay_pass || w != m_width || h != m_height) {
		m_width = w;
		m_height = h;
		rebuild_overlay_resources(w, h);
		Core::CanvasManager::get()->resize_layers(Core::UILayer::Editor, Core::UILayer::ScreenOverlay, w, h);
	}

	auto *r2d = m_overlay_r2d.get();
	auto *swapchain = ctx.swapchain;
	auto image_index = ctx.swapchain_image_index;
	auto *overlay_pass = m_overlay_pass.get();

	// ScreenOverlay + Editor onto swapchain with MSAA resolve, after SwapchainBlit
	graph.add_pass(
		"UIOverlay",
		[&ctx](Graphics::RG::RGPassBuilder &builder) {
			builder.mark_as_side_effect();
			builder.read_texture(ctx.h_display_color, Graphics::RG::ResourceState::ShaderRead);
		},
		[r2d, swapchain, image_index, overlay_pass, w, h](GFX::GfxCommandList &cmd, Graphics::RG::RGRegistry &) {
			const Mat4 ortho = glm::ortho(0.F, static_cast<float>(w), static_cast<float>(h), 0.F, -1.F, 1.F);
			const bool dirty =
				Core::CanvasManager::get()->is_any_layer_dirty(Core::UILayer::Editor, Core::UILayer::ScreenOverlay);

			overlay_pass->begin(cmd, swapchain, image_index);
			if (dirty) {
				PROFILE_SCOPE("UIOverlay::DirtyRebuild");
				r2d->begin_capture();
				r2d->begin(cmd, RHI::TextureFormat::BGRA8, RHI::SampleCount::X4, ortho);
				Core::CanvasManager::get()->render_layers(*r2d, cmd, Core::UILayer::Editor,
														  Core::UILayer::ScreenOverlay);
				r2d->end();
				Core::CanvasManager::get()->clear_layer_dirty_flags(Core::UILayer::Editor,
																	Core::UILayer::ScreenOverlay);
			} else {
				PROFILE_SCOPE("UIOverlay::Replay");
				r2d->execute_replay(cmd);
			}
			overlay_pass->end(cmd);
		});
}

void ViewRenderingSystem::on_resize(Uint32 width, Uint32 height) {
	Core::CanvasManager::get()->resize_layers(Core::UILayer::WorldSpace, Core::UILayer::ScreenCamera, width, height);
}

} // namespace Aquila::UI::Rendering
