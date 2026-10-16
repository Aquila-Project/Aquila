#include "Aquila/Rendering/Renderers/OverlayRenderer.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/GFX/GfxSwapchain.h"

namespace Aquila::Rendering {

void OverlayRenderer::on_init(GFX::GfxContext &ctx) {
	m_ctx = &ctx;
}

void OverlayRenderer::set_swapchain_target(GFX::GfxSwapchain &swapchain, Uint32 image_index) {
	m_swapchain = &swapchain;
	m_swapchain_image_index = image_index;
}

void OverlayRenderer::add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) {
	for (auto &sys : m_systems) {
		sys->add_passes(graph, ctx);
	}
}

void OverlayRenderer::blit_to_swapchain(Graphics::RG::RenderGraph &graph, FrameContext &ctx) {
	ctx.swapchain = m_swapchain;
	ctx.swapchain_image_index = m_swapchain_image_index;
	for (auto &sys : m_systems) {
		sys->blit_to_swapchain(graph, ctx);
	}
}

bool OverlayRenderer::replaces_swapchain() const {
	return std::ranges::any_of(m_systems, [](const Unique<IRenderingSystem> &sys) { return sys->replaces_swapchain(); });
}

void OverlayRenderer::on_resize(Uint32 width, Uint32 height) {
	for (auto &sys : m_systems) {
		sys->on_resize(width, height);
	}
}

void OverlayRenderer::on_shutdown() {
	for (auto &sys : m_systems) {
		sys->on_shutdown();
	}
}

} // namespace Aquila::Rendering
