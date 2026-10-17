#ifndef AQUILA_RENDERING_DEBUG_VIEW_SYSTEM_H
#define AQUILA_RENDERING_DEBUG_VIEW_SYSTEM_H

#include "Aquila/Graphics/Shader/ReloadablePipeline.h"
#include "Aquila/Rendering/Systems/RenderingSystemBase.h"

namespace Aquila::Rendering {

/// The non-Lit views from the Shading dropdown (albedo, normals, depth, wireframe...).
class DebugViewSystem : public RenderingSystemBase {
  public:
	DebugViewSystem() = default;
	~DebugViewSystem() override = default;

	void on_init(GFX::GfxContext &ctx) override;
	void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;

  private:
	Ref<Graphics::Shader::ReloadablePipeline> m_solid_pipeline;
	Ref<Graphics::Shader::ReloadablePipeline> m_wire_pipeline;
};

}

#endif
