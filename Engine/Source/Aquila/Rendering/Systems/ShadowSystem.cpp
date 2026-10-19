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

void ShadowSystem::on_shutdown() {
	m_bounds.clear();
	RenderingSystemBase::on_shutdown();
}

ShadowSystem::Sphere ShadowSystem::world_bounds(const Ref<Graphics::Resources::Mesh> &mesh, const Mat4 &model) {
	auto it = m_bounds.find(mesh.get());
	if (it == m_bounds.end() || it->second.mesh.lock() != mesh || it->second.revision != mesh->get_revision()) {
		std::erase_if(m_bounds, [](const auto &entry) { return entry.second.mesh.expired(); });
		Vec3 low(std::numeric_limits<F32>::max());
		Vec3 high(std::numeric_limits<F32>::lowest());
		for (const RHI::Vertex &vertex : mesh->get_vertices()) {
			low = Math::min(low, vertex.pos);
			high = Math::max(high, vertex.pos);
		}
		Sphere local;
		if (low.x <= high.x) {
			local.center = (low + high) * 0.5F;
			local.radius = Math::length(high - low) * 0.5F;
		}
		it = m_bounds.insert_or_assign(mesh.get(), MeshBounds{ .mesh = mesh, .revision = mesh->get_revision(), .local = local })
				 .first;
	}

	const Sphere &local = it->second.local;
	const F32 scale = Math::max(Math::length(Vec3(model[0])),
								Math::max(Math::length(Vec3(model[1])), Math::length(Vec3(model[2]))));
	return { .center = Vec3(model * Vec4(local.center, 1.F)), .radius = local.radius * scale };
}

Ref<std::vector<ShadowSystem::Caster>> ShadowSystem::collect_casters(FrameContext &ctx, bool with_bounds) {
	auto casters = std::make_shared<std::vector<Caster>>();
	auto &registry = ctx.scene->get_registry();
	auto view = registry.view<TransformComponent, MeshComponent>();
	for (auto entity : view) {
		if (!SceneManagement::is_visible_in_hierarchy(registry, entity)) {
			continue;
		}
		auto &mesh = view.get<MeshComponent>(entity);
		if (!mesh.is_valid() || !mesh.cast_shadows) {
			continue;
		}
		const Mat4 model = view.get<TransformComponent>(entity).get_world_matrix();
		casters->push_back({
			.gpu_mesh = get_or_upload_mesh(mesh.data),
			.model = model,
			.bounds = with_bounds ? world_bounds(mesh.data, model) : Sphere{},
		});
	}
	return casters;
}

void ShadowSystem::draw_caster(GFX::GfxCommandList &cmd, const Caster &caster, const Mat4 &view_proj,
							   DrawState &state) {
		GFX::GfxPipeline *pipeline = &m_pipeline->get();
		if (state.pipeline != pipeline) {
			cmd.bind_pipeline(*pipeline);
			state.pipeline = pipeline;
			state.material = nullptr;
		}
		ShadowPushConstants push_constants{ .model = caster.model, .cascade_view_proj = view_proj };
		cmd.push_constants(push_constants, RHI::ShaderStageFlags::Vertex);
	cmd.bind_vertex_buffer(caster.gpu_mesh->get_vertex_buffer());
	cmd.bind_index_buffer(caster.gpu_mesh->get_index_buffer());
	cmd.draw_indexed(caster.gpu_mesh->get_index_count());
}

void ShadowSystem::add_local_pass(RG::RenderGraph &graph, FrameContext &ctx) {
	auto *frame_data = ctx.frame_data;
	const RG::RGTextureHandle atlas =
		graph.import_texture(&frame_data->get_local_shadow_atlas(ctx.frame_slot), "LocalShadowAtlas");
	ctx.h_local_shadow_atlas = atlas;

	const auto &views = frame_data->get_local_shadow_views();
	const bool lit = ctx.settings == nullptr || ctx.settings->debug_view == DebugView::Lit;
	if (views.empty() || !lit || !m_pipeline || !m_pipeline->is_valid()) {
		return;
	}

	struct Tile {
		SceneFrameData::LocalShadowView view;
		std::vector<Uint32> casters;
	};

	auto casters = collect_casters(ctx, true);

	auto tiles = std::make_shared<std::vector<Tile>>();
	tiles->reserve(views.size());
	for (const SceneFrameData::LocalShadowView &shadow_view : views) {
		Tile tile{ .view = shadow_view, .casters = {} };
		for (Uint32 i = 0; i < casters->size(); ++i) {
			const Sphere &bounds = (*casters)[i].bounds;
			if (Math::length(bounds.center - shadow_view.light_position) <= shadow_view.range + bounds.radius) {
				tile.casters.push_back(i);
			}
		}
		tiles->push_back(std::move(tile));
	}

	graph.add_pass(
		"LocalShadows",
		[&ctx, atlas](RG::RGPassBuilder &builder) {
			ctx.h_local_shadow_atlas = builder.set_depth_attachment(
				atlas, RG::AttachmentLoadOp::Clear, RG::AttachmentStoreOp::Store, RG::AttachmentLoadOp::DontCare,
				RG::AttachmentStoreOp::DontCare, false, RG::ClearDepth{ .depth = 1.F });
		},
		[this, casters, tiles, frame_data, frame_slot = ctx.frame_slot](GFX::GfxCommandList &cmd, RG::RGRegistry &) {
			DrawState state{ .frame_data = frame_data, .frame_slot = frame_slot };
			for (const Tile &tile : *tiles) {
				const auto size = static_cast<F32>(tile.view.size);
				cmd.set_viewport(static_cast<F32>(tile.view.x), static_cast<F32>(tile.view.y), size, size);
				cmd.set_scissor(static_cast<Int32>(tile.view.x), static_cast<Int32>(tile.view.y), tile.view.size,
								tile.view.size);
				for (const Uint32 index : tile.casters) {
					draw_caster(cmd, (*casters)[index], tile.view.view_proj, state);
				}
			}
		});
}

void ShadowSystem::add_passes(RG::RenderGraph &graph, FrameContext &ctx) {
	add_local_pass(graph, ctx);

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

	auto draw_calls = collect_casters(ctx, false);

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
			[this, draw_calls, view_proj, size, frame_data, frame_slot](GFX::GfxCommandList &cmd, RG::RGRegistry &) {
				cmd.set_viewport(0.F, 0.F, size, size);
				cmd.set_scissor(0, 0, static_cast<Uint32>(size), static_cast<Uint32>(size));
				if (!m_pipeline || !m_pipeline->is_valid() || draw_calls->empty()) {
					return;
				}
				DrawState state{ .frame_data = frame_data, .frame_slot = frame_slot };
				for (const Caster &caster : *draw_calls) {
					draw_caster(cmd, caster, view_proj, state);
				}
			});
	}
}

} // namespace Aquila::Rendering
