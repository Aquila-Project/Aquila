#pragma once
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Rendering/Systems/RenderingSystemBase.h"
#include "Aquila/Graphics/Shader/ReloadablePipeline.h"

namespace Aquila::Rendering {

/// Cascaded shadow maps. Adds no passes when nothing casts shadows, Show > Shadows is off,
/// or the debug view doesn't need them.
class ShadowSystem : public RenderingSystemBase {
  public:
	ShadowSystem() = default;
	~ShadowSystem() override = default;

	void on_init(GFX::GfxContext &ctx) override;
	void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;

  private:
	Ref<Graphics::Shader::ReloadablePipeline> m_pipeline;
};

} // namespace Aquila::Rendering
