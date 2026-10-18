#include "Aquila/Rendering/Systems/ComputeTestSystem.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"

namespace Aquila::Rendering {

using Aquila::Foundation::EnginePaths::shaders_dir;
using AABB = Math::Geometry::AABB::AABB;

static constexpr Uint32 K_ELEMENT_COUNT = 3456; // cluster size so 16x9x24

struct GpuAABB {
	Vec4 min;
	Vec4 max;
};

void ClusterComputeSystem::on_init(GFX::GfxContext &ctx) {
	RenderingSystemBase::on_init(ctx);

	m_grid_data.grid.x = 16;
	m_grid_data.grid.y = 9;
	m_grid_data.grid.z = 24;

	m_storage_layout = ctx.create_descriptor_set_layout({
		.bindings = { {
						  .binding = 0,
						  .type = RHI::DescriptorType::StorageBuffer,
						  .stages = RHI::ShaderStageFlags::Compute,
						  .count = 1,
					  },
					  {
						  .binding = 1,
						  .type = RHI::DescriptorType::UniformBuffer,
						  .stages = RHI::ShaderStageFlags::Compute,
						  .count = 1,
					  } },
	});

	m_pipeline = Graphics::Shader::ReloadablePipeline::create(
		ctx, shaders_dir() + "ClusterCompute.slang", [this](GFX::GfxContext &build_ctx) -> Ref<GFX::GfxPipeline> {
			std::vector<RHI::VulkanCompiledStage> stages;
			std::string err;
			if (!RHI::VulkanShaderCompiler::compile_file(shaders_dir() + "ClusterCompute.slang", stages, err)) {
				AQUILA_LOG_ERROR("ClusterComputeSystem: shader compile failed: {}", err);
				return nullptr;
			}

			RHI::ComputePipelineDesc pipeline_desc{};
			pipeline_desc.compute_shader = {
				.stage = RHI::ShaderStageFlags::Compute,
				.spirv = stages[0].spirv,
				.entry_point = stages[0].entry_point_name,
			};
			pipeline_desc.set_layouts = { &SceneFrameData::get()->get_layout().get_rhi(), &m_storage_layout->get_rhi() };
			pipeline_desc.debug_name = "ClusterCompute";
			return build_ctx.create_compute_pipeline(pipeline_desc);
		});

	m_output_buffer = ctx.create_buffer({
		.size = sizeof(GpuAABB) * K_ELEMENT_COUNT,
		.usage = RHI::BufferUsage::StorageBuffer,
		.domain = RHI::MemoryDomain::GpuOnly,
		.debug_name = "ClusterCompute_AABBOutput",
	});

	m_grid_buffer = ctx.create_buffer({
		.size = sizeof(GridData),
		.usage = RHI::BufferUsage::UniformBuffer,
		.domain = RHI::MemoryDomain::CpuToGpu,
		.debug_name = "ClusteCompute_GridData",
	});

	m_grid_buffer->write(&m_grid_data);

	m_storage_set = ctx.allocate_descriptor_set(*m_storage_layout);
	m_storage_set->set_buffer(0, *m_output_buffer).flush();
	m_storage_set->set_buffer(1, *m_grid_buffer).flush();
}

void ClusterComputeSystem::add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) {
	if (!m_pipeline || !m_pipeline->is_valid()) {
		return;
	}

	auto *frame_data = ctx.frame_data;
	const Uint32 frame_slot = ctx.frame_slot;

	auto h_aab_bs =
		graph.import_buffer(m_output_buffer.get(), "ClusterAABBs", Graphics::RG::ResourceState::UnorderedAccess);

	graph.add_pass(
		"ClusterCompute", [&h_aab_bs](Graphics::RG::RGPassBuilder &builder) { h_aab_bs = builder.write_buffer(h_aab_bs); },
		[this, frame_data, frame_slot](GFX::GfxCommandList &cmd, Graphics::RG::RGRegistry &) {
			cmd.bind_pipeline(m_pipeline->get());
			cmd.bind_descriptor_set(0, frame_data->get_descriptor_set(frame_slot));
			cmd.bind_descriptor_set(1, *m_storage_set);
			cmd.dispatch((m_grid_data.grid.x + 4 - 1) / 4, (m_grid_data.grid.y + 4 - 1) / 4,
						 (m_grid_data.grid.z + 4 - 1) / 4);
		});

	ctx.h_cluster_aab_bs = h_aab_bs;
}

} // namespace Aquila::Rendering
