#include "Aquila/Rendering/Systems/LightCullingSystem.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/Graphics/RenderGraph/RGTypes.h"
#include "Aquila/RHI/Backend/RHITypes.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"

namespace Aquila::Rendering {

using Aquila::Foundation::EnginePaths::shaders_dir;

void LightCullingSystem::on_init(GFX::GfxContext &ctx) {
	RenderingSystemBase::on_init(ctx);

	m_storage_layout = ctx.create_descriptor_set_layout({
		.bindings = {
			{ .binding = 0, .type = RHI::DescriptorType::StorageBuffer, .stages = RHI::ShaderStageFlags::Compute, .count = 1 },
			{ .binding = 1, .type = RHI::DescriptorType::StorageBuffer, .stages = RHI::ShaderStageFlags::Compute, .count = 1 },
			{ .binding = 2, .type = RHI::DescriptorType::StorageBuffer, .stages = RHI::ShaderStageFlags::Compute, .count = 1 },
			{ .binding = 3, .type = RHI::DescriptorType::StorageBuffer, .stages = RHI::ShaderStageFlags::Compute, .count = 1 },
		},
	});

	m_pipeline = Graphics::Shader::ReloadablePipeline::create(
		ctx, shaders_dir() + "LightCullCompute.slang", [this](GFX::GfxContext &build_ctx) -> Ref<GFX::GfxPipeline> {
			std::vector<RHI::VulkanCompiledStage> stages;
			std::string err;
			if (!RHI::VulkanShaderCompiler::compile_file(shaders_dir() + "LightCullCompute.slang", stages, err)) {
				AQUILA_LOG_ERROR("LightCullingSystem: shader compile failed: {}", err);
				return nullptr;
			}

			RHI::ComputePipelineDesc pipeline_desc{};
			pipeline_desc.compute_shader = {
				.stage = RHI::ShaderStageFlags::Compute,
				.spirv = stages[0].spirv,
				.entry_point = stages[0].entry_point_name,
			};
			pipeline_desc.set_layouts = { &SceneFrameData::get()->get_layout().get_rhi(), &m_storage_layout->get_rhi() };
			pipeline_desc.debug_name = "LightCull";
			return build_ctx.create_compute_pipeline(pipeline_desc);
		});

	m_global_index_counter = ctx.create_buffer({
		.size = sizeof(Uint32),
		.usage = RHI::BufferUsage::StorageBuffer,
		.domain = RHI::MemoryDomain::GpuOnly,
		.debug_name = "GlobalIndexCounter",
	});

	auto *sfd = SceneFrameData::get();
	m_storage_set = ctx.allocate_descriptor_set(*m_storage_layout);
	m_storage_set->set_buffer(1, sfd->get_light_index_list_buffer())
		.set_buffer(2, sfd->get_cluster_light_info_buffer())
		.set_buffer(3, *m_global_index_counter)
		.flush();
}

void LightCullingSystem::add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) {
	if (!m_pipeline || !m_pipeline->is_valid()) {
		return;
	}

	auto *frame_data = ctx.frame_data;
	const Uint32 frame_slot = ctx.frame_slot;

	if (ctx.h_cluster_aab_bs.is_valid() && !m_aabb_buffer_bound) {
		GFX::GfxBuffer &aabb_buffer = graph.get_registry().get_buffer(ctx.h_cluster_aab_bs);
		m_storage_set->set_buffer(0, aabb_buffer).flush();
		m_aabb_buffer_bound = true;
	}

	auto h_aab_bs = ctx.h_cluster_aab_bs;
	auto h_light_list = graph.import_buffer(&SceneFrameData::get()->get_light_index_list_buffer(), "LightList",
										 Graphics::RG::ResourceState::UnorderedAccess);
	auto h_cluster_light_info = graph.import_buffer(&SceneFrameData::get()->get_cluster_light_info_buffer(), "ClusterLightInfo",
												Graphics::RG::ResourceState::UnorderedAccess);
	auto h_global_counter = graph.import_buffer(m_global_index_counter.get(), "GlobalIndexCounter",
											 Graphics::RG::ResourceState::UnorderedAccess);

	graph.add_pass(
		"LightCull",
		[&h_aab_bs, &h_light_list, &h_cluster_light_info, &h_global_counter](Graphics::RG::RGPassBuilder &builder) {
			builder.read_buffer(h_aab_bs, Graphics::RG::ResourceState::ShaderRead);
			h_light_list = builder.write_buffer(h_light_list);
			h_cluster_light_info = builder.write_buffer(h_cluster_light_info);
			h_global_counter = builder.write_buffer(h_global_counter);
		},
		[this, frame_data, frame_slot](GFX::GfxCommandList &cmd, Graphics::RG::RGRegistry &) {
			cmd.fill_buffer(*m_global_index_counter, 0u);
			cmd.bind_pipeline(m_pipeline->get());
			cmd.bind_descriptor_set(0, frame_data->get_descriptor_set(frame_slot));
			cmd.bind_descriptor_set(1, *m_storage_set);
			cmd.dispatch(4, 3, 6);
		});

	if (ctx.h_cluster_aab_bs.is_valid()) {
		ctx.h_light_list = h_light_list;
		ctx.h_cluster_light_info = h_cluster_light_info;
	}
}

} // namespace Aquila::Rendering
