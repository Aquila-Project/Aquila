#include "Aquila/Rendering/Systems/DebugViewSystem.h"
#include "Aquila/Foundation/EnginePaths.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/Scene/Components/MaterialComponent.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scene/Visibility.h"

namespace Aquila::Rendering {

using namespace SceneManagement::Components;
using namespace Graphics;
using Aquila::Foundation::EnginePaths::shaders_dir;

namespace {

struct DebugPushConstants {
	Mat4 model;
	Vec4 color = Vec4(1.F);
	Uint32 material_index = 0;
	Uint32 mode = 0;
};

Ref<Shader::ReloadablePipeline> make_pipeline(GFX::GfxContext &ctx, RHI::FillMode fill) {
	const std::string path = shaders_dir() + "DebugView.slang";
	return Shader::ReloadablePipeline::create(
		ctx, path, [path, fill](GFX::GfxContext &build_ctx) -> Ref<GFX::GfxPipeline> {
			std::vector<RHI::VulkanCompiledStage> stages;
			std::string err;
			if (!RHI::VulkanShaderCompiler::compile_file(path, stages, err)) {
				AQUILA_LOG_ERROR("DebugViewSystem: shader compile failed: {}", err);
				return nullptr;
			}

			RHI::GraphicsPipelineDesc desc{};
			for (auto &stage : stages) {
				RHI::ShaderStageDesc shader{ .spirv = stage.spirv, .entry_point = stage.entry_point_name };
				if (stage.stage == VK_SHADER_STAGE_VERTEX_BIT) {
					shader.stage = RHI::ShaderStageFlags::Vertex;
					desc.vertex_shader = shader;
				} else {
					shader.stage = RHI::ShaderStageFlags::Fragment;
					desc.fragment_shader = shader;
				}
			}

			desc.color_formats = { RHI::TextureFormat::RGBA16F };
			desc.depth_format = RHI::TextureFormat::Depth32;
			desc.topology = RHI::PrimitiveTopology::TriangleList;
			desc.raster.cull_mode = fill == RHI::FillMode::Wireframe ? RHI::CullMode::None : RHI::CullMode::Back;
			desc.raster.front_face = RHI::FrontFace::Clockwise;
			desc.raster.fill_mode = fill;
			desc.depth_stencil.depth_test = true;
			desc.depth_stencil.depth_write = true;
			desc.set_layouts = { &SceneFrameData::get()->get_layout().get_rhi() };
			desc.push_constants = { { .stages = RHI::ShaderStageFlags::Vertex | RHI::ShaderStageFlags::Fragment,
									  .offset = 0,
									  .size = sizeof(DebugPushConstants) } };
			return build_ctx.create_graphics_pipeline(desc);
		});
}

} // namespace

void DebugViewSystem::on_init(GFX::GfxContext &ctx) {
	RenderingSystemBase::on_init(ctx);
	m_solid_pipeline = make_pipeline(ctx, RHI::FillMode::Solid);
	m_wire_pipeline = make_pipeline(ctx, RHI::FillMode::Wireframe);
}

void DebugViewSystem::add_passes(RG::RenderGraph &graph, FrameContext &ctx) {
	if (ctx.settings == nullptr || ctx.settings->debug_view == DebugView::Lit) {
		return;
	}
	const DebugView mode = ctx.settings->debug_view;
	auto &pipeline = mode == DebugView::Wireframe ? m_wire_pipeline : m_solid_pipeline;
	if (!pipeline || !pipeline->is_valid()) {
		return;
	}

	struct DrawCall {
		Ref<GFX::GfxMesh> gpu_mesh;
		Mat4 model;
		Uint32 material_index = 0;
		Vec4 color = Vec4(1.F);
	};

	auto &registry = ctx.scene->get_registry();
	auto view = registry.view<TransformComponent, MeshComponent>();
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
		const auto *material = registry.try_get<MaterialComponent>(entity);
		draw_calls.push_back({
			.gpu_mesh = get_or_upload_mesh(mesh.data),
			.model = transform.get_world_matrix(),
			.material_index = material != nullptr ? material->material_index : 0,
			.color = material != nullptr ? Vec4(1.F) : Vec4(0.7F, 0.7F, 0.7F, 1.F),
		});
	}

	const Vec4 clear = mode == DebugView::Depth ? Vec4(0.F, 0.F, 0.F, 1.F) : Vec4(0.075F, 0.075F, 0.075F, 1.F);
	auto *frame_data = ctx.frame_data;
	const Uint32 frame_slot = ctx.frame_slot;
	auto *raw_pipeline = pipeline.get();

	graph.add_pass(
		"DebugView",
		[&ctx, clear](RG::RGPassBuilder &builder) {
			builder.read_buffer(ctx.h_light_list, RG::ResourceState::ShaderRead);
			builder.read_buffer(ctx.h_cluster_light_info, RG::ResourceState::ShaderRead);
			for (auto &shadow_handle : ctx.h_shadow_maps) {
				if (shadow_handle.is_valid()) {
					builder.read_texture(shadow_handle, RG::ResourceState::ShaderRead);
				}
			}
			ctx.h_scene_color = builder.set_color_attachment(0, ctx.h_scene_color, RG::AttachmentLoadOp::Clear,
															 RG::AttachmentStoreOp::Store, RG::ClearColor{ clear });
			ctx.h_depth = builder.set_depth_attachment(
				ctx.h_depth, RG::AttachmentLoadOp::Clear, RG::AttachmentStoreOp::Store, RG::AttachmentLoadOp::DontCare,
				RG::AttachmentStoreOp::DontCare, false, RG::ClearDepth{ .depth = 1.F });
		},
		[draw_calls = std::move(draw_calls), raw_pipeline, frame_data, frame_slot, mode](GFX::GfxCommandList &cmd,
																						 RG::RGRegistry &) {
			cmd.bind_pipeline(raw_pipeline->get());
			cmd.bind_descriptor_set(0, frame_data->get_descriptor_set(frame_slot));
			for (const auto &draw_call : draw_calls) {
				DebugPushConstants push{ .model = draw_call.model,
										 .color = draw_call.color,
										 .material_index = draw_call.material_index,
										 .mode = static_cast<Uint32>(mode) };
				cmd.push_constants(push, RHI::ShaderStageFlags::Vertex | RHI::ShaderStageFlags::Fragment);
				cmd.bind_vertex_buffer(draw_call.gpu_mesh->get_vertex_buffer());
				cmd.bind_index_buffer(draw_call.gpu_mesh->get_index_buffer());
				cmd.draw_indexed(draw_call.gpu_mesh->get_index_count());
			}
		});
}

} // namespace Aquila::Rendering
