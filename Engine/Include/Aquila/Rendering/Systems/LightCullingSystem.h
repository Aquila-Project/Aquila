#pragma once
#include "Aquila/Rendering/Systems/RenderingSystemBase.h"
#include "Aquila/Graphics/Shader/ReloadablePipeline.h"
#include "Aquila/GFX/GfxBuffer.h"
#include "Aquila/GFX/GfxDescriptorSet.h"

namespace Aquila::Rendering {

/// Assigns lights to the clusters, so shading only loops over the lights near each pixel.
class LightCullingSystem : public RenderingSystemBase {
  public:
	LightCullingSystem() = default;
	~LightCullingSystem() override = default;

	void on_init(GFX::GfxContext &ctx) override;
	void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;

  private:
	Ref<Graphics::Shader::ReloadablePipeline> m_pipeline;
	Ref<GFX::GfxDescriptorSetLayout> m_storage_layout;
	Ref<GFX::GfxBuffer> m_global_index_counter; // atomic counter reset each frame before dispatch
	Ref<GFX::GfxDescriptorSet> m_storage_set;
	bool m_aabb_buffer_bound = false;
};

} // namespace Aquila::Rendering
