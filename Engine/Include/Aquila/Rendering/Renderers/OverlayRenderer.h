#pragma once
#include "Aquila/Rendering/Renderers/IRenderer.h"
#include "Aquila/Rendering/Systems/Base/IRenderingSystem.h"

namespace Aquila::GFX {
class GfxSwapchain;
}

namespace Aquila::Rendering {

/// Draws 2D on top of the scene, straight into the swapchain image (the UI lives here).
class OverlayRenderer : public IRenderer {
  public:
	OverlayRenderer() = default;
	~OverlayRenderer() override = default;

	AQUILA_NONCOPYABLE(OverlayRenderer);
	AQUILA_NONMOVEABLE(OverlayRenderer);

	void on_init(GFX::GfxContext &ctx) override;
	void on_shutdown() override;
	void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;
	void blit_to_swapchain(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;
	[[nodiscard]] bool replaces_swapchain() const override;
	void on_resize(Uint32 width, Uint32 height) override;

	void set_swapchain_target(GFX::GfxSwapchain &swapchain, Uint32 image_index);

	template <typename T, typename... Args> T &add_system(Args &&...args) {
		static_assert(std::is_base_of_v<IRenderingSystem, T>);
		auto sys = std::make_unique<T>(std::forward<Args>(args)...);
		T &ref = *sys;
		sys->on_init(*m_ctx);
		m_systems.push_back(std::move(sys));
		return ref;
	}

  private:
	GFX::GfxContext *m_ctx = nullptr;
	std::vector<Unique<IRenderingSystem>> m_systems;

	GFX::GfxSwapchain *m_swapchain = nullptr;
	Uint32 m_swapchain_image_index = 0;
};

} // namespace Aquila::Rendering
