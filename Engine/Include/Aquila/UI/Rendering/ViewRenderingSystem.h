#pragma once

#include "Aquila/Rendering/Systems/Base/IRenderingSystem.h"
#include "Aquila/Graphics/Core/Renderer2D.h"
#include "Aquila/GFX/GfxRenderpass.h"
#include "Aquila/GFX/GfxTexture.h"

namespace Aquila::UI::Rendering {

/// Draws the Editor and ScreenOverlay canvases into the swapchain with 4x MSAA.
/// When no canvas changed it replays last frame's draws instead of rebuilding them.
class ViewRenderingSystem final : public Aquila::Rendering::IRenderingSystem {
  public:
	ViewRenderingSystem() = default;

	void on_init(GFX::GfxContext &ctx) override;
	void add_passes(Graphics::RG::RenderGraph &graph, Aquila::Rendering::FrameContext &ctx) override;
	void blit_to_swapchain(Graphics::RG::RenderGraph &graph, Aquila::Rendering::FrameContext &ctx) override;
	[[nodiscard]] bool replaces_swapchain() const override { return true; }
	void on_resize(Uint32 width, Uint32 height) override;
	void on_shutdown() override {}

  private:
	void rebuild_overlay_resources(Uint32 w, Uint32 h);

	GFX::GfxContext *m_ctx = nullptr;
	Unique<Graphics::Renderer2D> m_r2_d;
	Unique<Graphics::Renderer2D> m_overlay_r2d;
	Ref<GFX::GfxTexture> m_msaa_color;
	Ref<GFX::GfxRenderPass> m_overlay_pass;
	Uint32 m_width = 0;
	Uint32 m_height = 0;
};

} // namespace Aquila::UI::Rendering
