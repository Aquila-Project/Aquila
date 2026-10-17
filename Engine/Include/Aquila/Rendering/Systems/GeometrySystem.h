#pragma once
#include "Aquila/Rendering/Systems/RenderingSystemBase.h"

namespace Aquila::Rendering {

/// Draws every mesh with its material. Keeps requesting frames while a drawn material uses time.
class GeometrySystem : public RenderingSystemBase {
  public:
	GeometrySystem() = default;
	~GeometrySystem() override = default;

	void on_init(GFX::GfxContext &ctx) override;
	void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;
};

} // namespace Aquila::Rendering
