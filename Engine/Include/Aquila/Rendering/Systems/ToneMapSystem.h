#ifndef AQUILA_RENDERING_TONE_MAP_SYSTEM_H
#define AQUILA_RENDERING_TONE_MAP_SYSTEM_H

#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/GFX/GfxDescriptorSet.h"
#include "Aquila/Graphics/Shader/ReloadablePipeline.h"
#include "Aquila/Rendering/Systems/RenderingSystemBase.h"

namespace Aquila::Rendering {

/// Turns the exposed HDR scene color into the display color: RenderSettings::tone_mapper, then the sRGB encoding.
/// Reads FrameContext::h_bloom_final instead when BloomSystem ran. Debug views are copied as they are. Add it after
/// everything that draws lit HDR (geometry, sky, bloom) and before the overlays (grid, outline), which then draw into
/// the display color.
class ToneMapSystem : public RenderingSystemBase {
  public:
	ToneMapSystem() = default;
	~ToneMapSystem() override = default;

	void on_init(GFX::GfxContext &ctx) override;
	void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;

  private:
	Ref<GFX::GfxDescriptorSetLayout> m_layout;
	std::array<Ref<GFX::GfxDescriptorSet>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_sets;
	Ref<Graphics::Shader::ReloadablePipeline> m_pipeline;
};

}

#endif
