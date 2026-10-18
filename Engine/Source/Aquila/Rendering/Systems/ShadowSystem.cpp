#include "Aquila/Rendering/Systems/ShadowSystem.h"
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

struct ShadowPushConstants {
	Mat4 model;
	Mat4 cascade_view_proj;
};

void ShadowSystem::on_init(GFX::GfxContext &ctx) {
	RenderingSystemBase::on_init(ctx);

	m_pipeline = Shader::ReloadablePipeline::create(
		ctx, shaders_dir() + "ShadowDepth.slang", [](GFX::GfxContext &build_ctx) -> Ref<GFX::GfxPipeline> {
			std::vector<RHI::VulkanCompiledStage> stages;
			std::string err;
			if (!RHI::VulkanShaderCompiler::compile_file(shaders_dir() + "ShadowDepth.slang", stages, err)) {
				AQUILA_LOG_ERROR("ShadowSystem: shader compile failed: {}", err);
				return nullptr;
			}

			RHI::GraphicsPipelineDesc pipeline_descriptor{};
			for (auto &stage : stages) {
				if (stage.stage == VK_SHADER_STAGE_VERTEX_BIT) {
					pipeline_descriptor.vertex_shader = {
						.stage = RHI::ShaderStageFlags::Vertex,
						.spirv = stage.spirv,
						.entry_point = stage.entry_point_name,
					};
				}
			}

			pipeline_descriptor.color_formats = {};
			pipeline_descriptor.depth_format = RHI::TextureFormat::Depth32;
			pipeline_descriptor.topology = RHI::PrimitiveTopology::TriangleList;
			pipeline_descriptor.raster.cull_mode = RHI::CullMode::None;
			pipeline_descriptor.depth_stencil.depth_test = true;
			pipeline_descriptor.depth_stencil.depth_write = true;
			pipeline_descriptor.push_constants = { { RHI::ShaderStageFlags::Vertex, 0, sizeof(ShadowPushConstants) } };
			return build_ctx.create_graphics_pipeline(pipeline_descriptor);
		});
}

void ShadowSystem::add_passes(RG::RenderGraph &graph, FrameContext &ctx) {
	struct DrawCall {
		Ref<GFX::GfxMesh> gpu_mesh;
		Mat4 model;
	};

	auto *frame_data = ctx.frame_data;
	const Uint32 frame_slot = ctx.frame_slot;
	const bool visible_mode = ctx.settings == nullptr || ctx.settings->debug_view == DebugView::Lit ||
		ctx.settings->debug_view == DebugView::ShadowCascades;
	if (!frame_data->shadows_enabled() || !visible_mode) {
		for (Uint32 cascade = 0; cascade < SHADOW_CASCADE_COUNT; ++cascade) {
			ctx.h_shadow_maps[cascade] =
				graph.import_texture(&frame_data->get_shadow_map(frame_slot, cascade), "ShadowMap");
		}
		return;
	}

	auto draw_calls = std::make_shared<std::vector<DrawCall>>();
	{
		auto &registry = ctx.scene->get_registry();
		auto view = registry.view<TransformComponent, MeshComponent>();
		draw_calls->reserve(view.size_hint());
		for (auto entity : view) {
			if (!SceneManagement::is_visible_in_hierarchy(registry, entity)) {
				continue;
			}
			auto &transform = view.get<TransformComponent>(entity);
			auto &mesh = view.get<MeshComponent>(entity);
			if (!mesh.is_valid() || !mesh.cast_shadows) {
				continue;
			}
			draw_calls->push_back({
				.gpu_mesh = get_or_upload_mesh(mesh.data),
				.model = transform.get_world_matrix(),
			});
		}
	}

	const F32 size = static_cast<F32>(SceneFrameData::kShadowMapSize);
	const auto &cascade_vp = frame_data->get_cascade_view_proj();

	for (Uint32 cascade = 0; cascade < SHADOW_CASCADE_COUNT; ++cascade) {
		RG::RGTextureHandle handle =
			graph.import_texture(&frame_data->get_shadow_map(frame_slot, cascade), "ShadowMap");
		const Mat4 view_proj = cascade_vp[cascade];

		graph.add_pass(
			"ShadowCascade" + std::to_string(cascade),
			[&ctx, cascade, handle](RG::RGPassBuilder &builder) {
				ctx.h_shadow_maps[cascade] = builder.set_depth_attachment(
					handle, RG::AttachmentLoadOp::Clear, RG::AttachmentStoreOp::Store, RG::AttachmentLoadOp::DontCare,
					RG::AttachmentStoreOp::DontCare, /*readOnly=*/false, RG::ClearDepth{ .depth = 1.F });
			},
			[this, draw_calls, view_proj, size](GFX::GfxCommandList &cmd, RG::RGRegistry &) {
				cmd.set_viewport(0.F, 0.F, size, size);
				cmd.set_scissor(0, 0, static_cast<Uint32>(size), static_cast<Uint32>(size));
				if (!m_pipeline || !m_pipeline->is_valid() || draw_calls->empty()) {
					return;
				}
				cmd.bind_pipeline(m_pipeline->get());
				for (const auto &draw_call : *draw_calls) {
					ShadowPushConstants push_constants{ .model = draw_call.model, .cascade_view_proj = view_proj };
					cmd.push_constants(push_constants, RHI::ShaderStageFlags::Vertex);
					cmd.bind_vertex_buffer(draw_call.gpu_mesh->get_vertex_buffer());
					cmd.bind_index_buffer(draw_call.gpu_mesh->get_index_buffer());
					cmd.draw_indexed(draw_call.gpu_mesh->get_index_count());
				}
			});
	}
}

} // namespace Aquila::Rendering
