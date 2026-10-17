#pragma once
#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::GFX {
class GfxContext;
}

namespace Aquila::Graphics::RG {
class RenderGraph;
}

namespace Aquila::Rendering {
struct FrameContext;

/// A group of rendering systems with its own targets.
/// RenderPipeline runs the renderers in the order they were added.
class IRenderer {
  public:
	virtual ~IRenderer() = default;

	AQUILA_NONCOPYABLE(IRenderer);
	AQUILA_NONMOVEABLE(IRenderer);

	virtual void on_init(GFX::GfxContext &ctx) = 0;
	virtual void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) = 0;
	virtual void blit_to_swapchain(Graphics::RG::RenderGraph & /*graph*/, FrameContext & /*ctx*/) {}
	[[nodiscard]] virtual bool replaces_swapchain() const { return false; }
	virtual void on_resize(Uint32 width, Uint32 height) {}
	virtual void on_shutdown() {}

  protected:
	IRenderer() = default;
};

} // namespace Aquila::Rendering
