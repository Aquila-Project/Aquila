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

/// One feature of the renderer (shadows, sky, grid...). Every frame add_passes() puts its passes
/// into the render graph. Add one with Renderer::add_system<T>(), which calls on_init() right away.
///
/// @code
/// class FogSystem : public RenderingSystemBase {
///   public:
///     void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override {
///         graph.add_pass(
///             "Fog",
///             [&](Graphics::RG::RGPassBuilder &builder) {
///                 ctx.h_scene_color = builder.set_color_attachment(0, ctx.h_scene_color, AttachmentLoadOp::Load);
///             },
///             [](GFX::GfxCommandList &cmd, Graphics::RG::RGRegistry &) { cmd.draw(3); });
///     }
/// };
/// @endcode
class IRenderingSystem {
  public:
	virtual ~IRenderingSystem() = default;

	AQUILA_NONCOPYABLE(IRenderingSystem);
	AQUILA_NONMOVEABLE(IRenderingSystem);

	virtual void on_init(GFX::GfxContext &ctx) = 0;
	virtual void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) = 0;
	/// Runs after every system's add_passes(), for passes that draw into the swapchain image.
	virtual void blit_to_swapchain(Graphics::RG::RenderGraph & /*graph*/, FrameContext & /*ctx*/) {}
	/// True when it overwrites the whole swapchain image, so the scene blit is skipped.
	[[nodiscard]] virtual bool replaces_swapchain() const { return false; }
	virtual void on_resize(Uint32 /*width*/, Uint32 /*height*/) {}
	virtual void on_shutdown() {}

  protected:
	IRenderingSystem() = default;
};

} // namespace Aquila::Rendering
