#include "Aquila/Rendering/Systems/DepthPrepassSystem.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Scene/Visibility.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"

namespace Aquila::Rendering {

using namespace SceneManagement::Components;
using namespace Graphics;
using Aquila::Foundation::EnginePaths::shaders_dir;

struct DepthPushConstants {
	Mat4 model;
};

void DepthPrepassSystem::on_init(GFX::GfxContext &ctx) {
	RenderingSystemBase::on_init(ctx);

	m_pipeline = Shader::ReloadablePipeline::create(
		ctx, shaders_dir() + "DepthOnly.slang", [](GFX::GfxContext &build_ctx) -> Ref<GFX::GfxPipeline> {
			std::vector<RHI::VulkanCompiledStage> stages;
			std::string err;
			if (!RHI::VulkanShaderCompiler::compile_file(shaders_dir() + "DepthOnly.slang", stages, err)) {
				AQUILA_LOG_ERROR("DepthPrepassSystem: shader compile failed: {}", err);
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

			pipeline_descriptor.color_formats = {};
			pipeline_descriptor.depth_format = RHI::TextureFormat::Depth32;
			pipeline_descriptor.topology = RHI::PrimitiveTopology::TriangleList;
			pipeline_descriptor.raster.cull_mode = RHI::CullMode::Back;
			pipeline_descriptor.raster.front_face = RHI::FrontFace::Clockwise;
			pipeline_descriptor.depth_stencil.depth_test = true;
			pipeline_descriptor.depth_stencil.depth_write = true;
			pipeline_descriptor.set_layouts = { &SceneFrameData::get()->get_layout().get_rhi() };
			pipeline_descriptor.push_constants = { { RHI::ShaderStageFlags::Vertex, 0, sizeof(DepthPushConstants) } };
			return build_ctx.create_graphics_pipeline(pipeline_descriptor);
		});
}

void DepthPrepassSystem::add_passes(RG::RenderGraph &graph, FrameContext &ctx) {
	if (!m_pipeline || !m_pipeline->is_valid()) {
		return;
	}

	auto &registry = ctx.scene->get_registry();
	auto view = registry.view<TransformComponent, MeshComponent>();

	struct DrawCall {
		Ref<GFX::GfxMesh> gpu_mesh;
		Mat4 model;
	};

	std::vector<DrawCall> draw_calls;
	draw_calls.reserve(view.size_hint());

	for (auto entity : view) {
		if (!SceneManagement::is_visible_in_hierarchy(registry, entity)) {
			continue;
		}
		auto &transform = view.get<TransformComponent>(entity);
		auto &mesh = view.get<MeshComponent>(entity);
		if (!mesh.is_valid()) {
			continue;
		}

		draw_calls.push_back({
			.gpu_mesh = get_or_upload_mesh(mesh.data),
			.model = transform.get_world_matrix(),
		});
	}

	auto *frame_data = ctx.frame_data;
	const Uint32 frame_slot = ctx.frame_slot;

	graph.add_pass(
		"DepthPrepass",
		[&ctx](RG::RGPassBuilder &builder) {
			ctx.h_depth =
				builder.set_depth_attachment(ctx.h_depth, RG::AttachmentLoadOp::Clear, RG::AttachmentStoreOp::Store,
											 RG::AttachmentLoadOp::DontCare, RG::AttachmentStoreOp::DontCare,
											 /*readOnly=*/false, RG::ClearDepth{ .depth = 1.F });
		},
		[this, draw_calls = std::move(draw_calls), frame_data, frame_slot](GFX::GfxCommandList &cmd, RG::RGRegistry &) {
			if (draw_calls.empty()) {
				return;
			}
			cmd.bind_pipeline(m_pipeline->get());
			cmd.bind_descriptor_set(0, frame_data->get_descriptor_set(frame_slot));
			for (const auto &draw_call : draw_calls) {
				DepthPushConstants push_constants{ .model = draw_call.model };
				cmd.push_constants(push_constants, RHI::ShaderStageFlags::Vertex);
				cmd.bind_vertex_buffer(draw_call.gpu_mesh->get_vertex_buffer());
				cmd.bind_index_buffer(draw_call.gpu_mesh->get_index_buffer());
				cmd.draw_indexed(draw_call.gpu_mesh->get_index_count());
			}
		});
}

} // namespace Aquila::Rendering
