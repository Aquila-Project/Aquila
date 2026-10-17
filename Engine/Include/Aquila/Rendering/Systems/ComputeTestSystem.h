#pragma once
#include "Aquila/Rendering/Systems/RenderingSystemBase.h"
#include "Aquila/Foundation/Math/Geometry/AABB.h"
#include "Aquila/Graphics/Shader/ReloadablePipeline.h"
#include "Aquila/GFX/GfxBuffer.h"
#include "Aquila/GFX/GfxDescriptorSet.h"

namespace Aquila::Rendering {

/// Builds the cluster AABBs (CLUSTER_GRID_X * Y * Z) that LightCullingSystem fills with lights.
class ClusterComputeSystem : public RenderingSystemBase {
	struct GridData {
		Ivec3 grid;
	};

  public:
	ClusterComputeSystem() = default;
	~ClusterComputeSystem() override = default;

	void on_init(GFX::GfxContext &ctx) override;
	void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;

  private:
	Ref<Graphics::Shader::ReloadablePipeline> m_pipeline;
	Ref<GFX::GfxDescriptorSetLayout> m_storage_layout;
	Ref<GFX::GfxBuffer> m_output_buffer;
	Ref<GFX::GfxBuffer> m_grid_buffer;
	Ref<GFX::GfxDescriptorSet> m_storage_set;
	GridData m_grid_data{};
};

} // namespace Aquila::Rendering
