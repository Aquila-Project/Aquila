#include "Aquila/Rendering/Systems/ObjectPickingSystem.h"
#include "Aquila/Scene/Visibility.h"
#include "Aquila/Graphics/RenderGraph/RGTypes.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Foundation/FrameScheduler.h"

namespace Aquila::Rendering {

using namespace Graphics;
using namespace SceneManagement::Components;

using Aquila::SharedConstants::SHADERS_DIR;

struct PickingPushConstants {
	Mat4 model;
	Uint32 object_id;
};

void ObjectPickingSystem::on_init(GFX::GfxContext &ctx) {
	RenderingSystemBase::on_init(ctx);

	for (Uint32 slot = 0; slot < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++slot) {
		m_readback_buffers[slot] = ctx.create_buffer({
			.size = sizeof(Uint32),
			.usage = RHI::BufferUsage::TransferDst,
			.domain = RHI::MemoryDomain::GpuToCpu,
			.debug_name = "ObjectPickingReadback",
		});
	}

	m_pipeline = Shader::ReloadablePipeline::create(
		ctx, SHADERS_DIR + "ObjectPicking.slang", [](GFX::GfxContext &build_ctx) -> Ref<GFX::GfxPipeline> {
			std::vector<RHI::VulkanCompiledStage> stages;
			std::string err;

			if (!RHI::VulkanShaderCompiler::compile_file(SHADERS_DIR + "ObjectPicking.slang", stages, err)) {
				AQUILA_LOG_ERROR("ObjectPickingSystem: shader compile failed: {}", err);
				return nullptr;
			}

			RHI::GraphicsPipelineDesc pipeline_descriptor{};

			for (auto &stage : stages) {
				RHI::ShaderStageDesc shader_descriptor{ .spirv = stage.spirv, .entry_point = stage.entry_point_name };

				if (stage.stage == VK_SHADER_STAGE_VERTEX_BIT) {
					shader_descriptor.stage = RHI::ShaderStageFlags::Vertex;
					pipeline_descriptor.vertex_shader = shader_descriptor;
				} else if (stage.stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
					shader_descriptor.stage = RHI::ShaderStageFlags::Fragment;
					pipeline_descriptor.fragment_shader = shader_descriptor;
				}
			}

			pipeline_descriptor.topology = RHI::PrimitiveTopology::TriangleList;

			pipeline_descriptor.raster.cull_mode = RHI::CullMode::None;

			pipeline_descriptor.depth_stencil.depth_test = true;
			pipeline_descriptor.depth_stencil.depth_write = false;
			pipeline_descriptor.depth_stencil.depth_compare = RHI::CompareOp::LessEqual;

			// Picking is an integer target.
			pipeline_descriptor.blend_attachments = { RHI::BlendAttachmentDesc{
				.enable = false,
			} };

			pipeline_descriptor.color_formats = { RHI::TextureFormat::R32UI };

			pipeline_descriptor.depth_format = RHI::TextureFormat::Depth32;

			pipeline_descriptor.set_layouts = { &SceneFrameData::get()->get_layout().get_rhi() };
			pipeline_descriptor.push_constants = { { .stages = RHI::ShaderStageFlags::Vertex |
														 RHI::ShaderStageFlags::Fragment,
													 .offset = 0,
													 .size = sizeof(PickingPushConstants) } };
			return build_ctx.create_graphics_pipeline(pipeline_descriptor);
		});
}

void ObjectPickingSystem::request_pick(Uint32 x, Uint32 y) {
	m_pending_request = PickRequest{ .x = x, .y = y };
}

void ObjectPickingSystem::resolve_readback(Uint32 frame_slot, SceneManagement::Scene &scene) {
	if (!m_readback_in_flight[frame_slot]) {
		return;
	}
	m_readback_in_flight[frame_slot] = false;

	auto &buffer = *m_readback_buffers[frame_slot];
	buffer.invalidate();

	const auto *mapped = static_cast<const Uint32 *>(buffer.map());
	const Uint32 encoded = (mapped != nullptr) ? *mapped : 0;
	buffer.unmap();

	if (encoded == 0) {
		on_picked(SceneManagement::Entity::null());
		return;
	}

	const auto handle = static_cast<entt::entity>(encoded - 1);
	if (!scene.get_registry().valid(handle)) {
		on_picked(SceneManagement::Entity::null());
		return;
	}

	on_picked(SceneManagement::Entity(handle, &scene));
}

void ObjectPickingSystem::add_passes(RG::RenderGraph &graph, FrameContext &ctx) {
	const Uint32 frame_slot = ctx.frame_slot;

	resolve_readback(frame_slot, *ctx.scene);

	if (std::ranges::any_of(m_readback_in_flight, [](bool in_flight) { return in_flight; })) {
		Foundation::FrameScheduler::get()->request_frame();
	}

	if (!m_pending_request.has_value()) {
		return;
	}

	if (!m_pipeline || !m_pipeline->is_valid()) {
		m_pending_request.reset();
		return;
	}

	const PickRequest request = *m_pending_request;
	m_pending_request.reset();

	if (request.x >= ctx.width || request.y >= ctx.height) {
		on_picked(SceneManagement::Entity::null());
		return;
	}

	auto &registry = ctx.scene->get_registry();

	auto view = registry.view<TransformComponent, MeshComponent>();

	struct DrawCall {
		Ref<GFX::GfxMesh> gpu_mesh;
		Mat4 model;
		Uint32 object_id;
	};

	std::vector<DrawCall> draw_calls;

	for (auto entity : view) {
		if (!SceneManagement::is_visible_in_hierarchy(registry, entity)) {
			continue;
		}
		auto &transform = view.get<TransformComponent>(entity);

		auto &mesh = view.get<MeshComponent>(entity);

		if (!mesh.is_valid()) {
			continue;
		}

		auto gpu_mesh = get_or_upload_mesh(mesh.data);

		if (!gpu_mesh) {
			continue;
		}

		draw_calls.push_back({
			.gpu_mesh = gpu_mesh,
			.model = transform.get_world_matrix(),
			.object_id = static_cast<Uint32>(entity) + 1,
		});
	}

	auto *frame_data = ctx.frame_data;

	graph.add_pass(
		"ObjectPicking",

		[&ctx](RG::RGPassBuilder &builder) {
			ctx.h_object_picking =
				builder.set_color_attachment(0, ctx.h_object_picking, RG::AttachmentLoadOp::Clear,
											 RG::AttachmentStoreOp::Store, { Foundation::Color::RGBA::BLACK });

			ctx.h_depth =
				builder.set_depth_attachment(ctx.h_depth, RG::AttachmentLoadOp::Load, RG::AttachmentStoreOp::Store,
											 RG::AttachmentLoadOp::DontCare, RG::AttachmentStoreOp::DontCare,
											 /*read_only=*/true);
		},

		[draw_calls = std::move(draw_calls), frame_data, frame_slot, this](GFX::GfxCommandList &cmd, RG::RGRegistry &) {
			cmd.bind_pipeline(m_pipeline->get());
			cmd.bind_descriptor_set(0, frame_data->get_descriptor_set(frame_slot));
			for (const auto &dc : draw_calls) {
				PickingPushConstants push{ .model = dc.model, .object_id = dc.object_id };
				cmd.push_constants(push, RHI::ShaderStageFlags::Vertex | RHI::ShaderStageFlags::Fragment);
				cmd.bind_vertex_buffer(dc.gpu_mesh->get_vertex_buffer());
				cmd.bind_index_buffer(dc.gpu_mesh->get_index_buffer());
				cmd.draw_indexed(dc.gpu_mesh->get_index_count());
			}
		});

	const RG::RGBufferHandle h_readback =
		graph.import_buffer(m_readback_buffers[frame_slot].get(), "ObjectPickingReadback");

	const RG::RGTextureHandle h_picking = ctx.h_object_picking;

	graph.add_pass(
		"ObjectPickingReadback",

		[h_picking, h_readback](RG::RGPassBuilder &builder) {
			builder.read_texture(h_picking, RG::ResourceState::TransferSrc);
			AQUILA_UNUSED(builder.write_buffer(h_readback, RG::ResourceState::TransferDst));
		},

		[h_picking, h_readback, request](GFX::GfxCommandList &cmd, RG::RGRegistry &registry) {
			auto &readback = registry.get_buffer(h_readback);

			cmd.copy_texture_to_buffer(registry.get_texture(h_picking), readback, 1, 1, 0, 0,
									   static_cast<Int32>(request.x), static_cast<Int32>(request.y));

			cmd.transition_buffer(readback, RG::ResourceState::TransferDst, RG::ResourceState::HostRead);
		});

	m_readback_in_flight[frame_slot] = true;
	Foundation::FrameScheduler::get()->request_frame();
}

} // namespace Aquila::Rendering
