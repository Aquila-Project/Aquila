#pragma once

#include "Aquila/Rendering/Systems/RenderingSystemBase.h"
#include "Aquila/Graphics/Shader/ReloadablePipeline.h"

namespace Aquila::Rendering {

/// Outlines every entity that has an OutlineComponent.
class OutlineSystem : public RenderingSystemBase {
  public:
	OutlineSystem() = default;
	~OutlineSystem() override = default;

	void on_init(GFX::GfxContext &ctx) override;
	void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;

  private:
	Ref<Graphics::Shader::ReloadablePipeline> m_mask_pipeline;
	Ref<Graphics::Shader::ReloadablePipeline> m_outline_pipeline;

	Ref<GFX::GfxDescriptorSetLayout> m_outline_layout;
	std::array<Ref<GFX::GfxDescriptorSet>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_outline_sets;
};

}
