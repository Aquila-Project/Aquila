#include "Aquila/Rendering/Systems/GridSystem.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"

namespace Aquila::Rendering {

using namespace Graphics;
using Aquila::Foundation::EnginePaths::shaders_dir;

void GridSystem::on_init(GFX::GfxContext &ctx) {
	RenderingSystemBase::on_init(ctx);

	m_pipeline = Shader::ReloadablePipeline::create(
		ctx, shaders_dir() + "Grid.slang", [](GFX::GfxContext &build_ctx) -> Ref<GFX::GfxPipeline> {
			std::vector<RHI::VulkanCompiledStage> stages;
			std::string err;
			if (!RHI::VulkanShaderCompiler::compile_file(shaders_dir() + "Grid.slang", stages, err)) {
				AQUILA_LOG_ERROR("GridSystem: shader compile failed: {}", err);
				return nullptr;
			}

			RHI::GraphicsPipelineDesc pipeline_descriptor{};
			for (auto &stage : stages) {
				RHI::ShaderStageDesc shader_descriptor{ .spirv = stage.spirv, .entry_point = stage.entry_point_name };
				if (stage.stage == VK_SHADER_STAGE_VERTEX_BIT) {
					shader_descriptor.stage = RHI::ShaderStageFlags::Vertex;
					pipeline_descriptor.vertex_shader = shader_descriptor;
				} else {
					shader_descriptor.stage = RHI::ShaderStageFlags::Fragment;
					pipeline_descriptor.fragment_shader = shader_descriptor;
				}
			}

			pipeline_descriptor.no_vertex_input = true;
			pipeline_descriptor.topology = RHI::PrimitiveTopology::TriangleList;
			pipeline_descriptor.raster.cull_mode = RHI::CullMode::None;
			pipeline_descriptor.depth_stencil.depth_test = true;
			pipeline_descriptor.depth_stencil.depth_write = false;
			pipeline_descriptor.depth_stencil.depth_compare = RHI::CompareOp::LessEqual;
			pipeline_descriptor.blend_attachments = { RHI::BlendAttachmentDesc{
				.enable = true,
				.src_color = RHI::BlendFactor::One,
				.dst_color = RHI::BlendFactor::OneMinusSrcAlpha,
				.color_op = RHI::BlendOp::Add,
				.src_alpha = RHI::BlendFactor::One,
				.dst_alpha = RHI::BlendFactor::OneMinusSrcAlpha,
				.alpha_op = RHI::BlendOp::Add,
			} };
			pipeline_descriptor.color_formats = { RHI::TextureFormat::RGBA16F };
			pipeline_descriptor.depth_format = RHI::TextureFormat::Depth32;
			pipeline_descriptor.set_layouts = { &SceneFrameData::get()->get_layout().get_rhi() };
			return build_ctx.create_graphics_pipeline(pipeline_descriptor);
		});
}

void GridSystem::add_passes(RG::RenderGraph &graph, FrameContext &ctx) {
	if (!m_pipeline || !m_pipeline->is_valid()) {
		return;
	}
	if (ctx.settings != nullptr && !ctx.settings->show_grid) {
		return;
	}

	auto *frame_data = ctx.frame_data;
	const Uint32 frame_slot = ctx.frame_slot;

	graph.add_pass(
		"Grid",
		[&ctx](RG::RGPassBuilder &builder) {
			ctx.h_scene_color = builder.set_color_attachment(0, ctx.h_scene_color, RG::AttachmentLoadOp::Load,
															 RG::AttachmentStoreOp::Store);

			ctx.h_depth = builder.set_depth_attachment(
				ctx.h_depth, RG::AttachmentLoadOp::Load, RG::AttachmentStoreOp::Store,
				RG::AttachmentLoadOp::DontCare, RG::AttachmentStoreOp::DontCare, /*read_only=*/true);
		},
		[this, frame_data, frame_slot](GFX::GfxCommandList &cmd, RG::RGRegistry &) {
			cmd.bind_pipeline(m_pipeline->get());
			cmd.bind_descriptor_set(0, frame_data->get_descriptor_set(frame_slot));
			cmd.draw(3);
		});
}

} // namespace Aquila::Rendering
