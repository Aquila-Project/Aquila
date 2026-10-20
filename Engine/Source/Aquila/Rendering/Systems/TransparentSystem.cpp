#include "Aquila/Rendering/Systems/TransparentSystem.h"
#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Rendering/MeshPushConstants.h"
#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/Scene/Components/MaterialComponent.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/DefaultScenes.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scene/Visibility.h"

namespace Aquila::Rendering {

using namespace SceneManagement::Components;
using namespace Graphics;

void TransparentSystem::add_passes(RG::RenderGraph &graph, FrameContext &ctx) {
	if (ctx.scene == nullptr || (ctx.settings != nullptr && ctx.settings->debug_view != DebugView::Lit)) {
		return;
	}

	struct Draw {
		Ref<Material> material;
		Ref<GFX::GfxMesh> gpu_mesh;
		Mat4 model;
		Uint32 material_index = 0;
		Uint32 receives_shadows = 0;
		F32 distance = 0.F;
	};

	auto &registry = ctx.scene->get_registry();
	std::vector<Draw> draws;
	bool animated = false;
	auto view = registry.view<TransformComponent, MeshComponent, MaterialComponent>();
	for (auto entity : view) {
		if (!SceneManagement::is_visible_in_hierarchy(registry, entity)) {
			continue;
		}
		auto &mesh = view.get<MeshComponent>(entity);
		if (!mesh.is_valid()) {
			continue;
		}
		auto &material = view.get<MaterialComponent>(entity);
		SceneManagement::ensure_material(material, *m_ctx);
		if (!material.material || material.material->get_blend_mode() != BlendMode::Transparent ||
			material.material->get_domain() == Shader::ShaderDomain::Fullscreen) {
			continue;
		}
		animated = animated || material.material->uses_time();
		const Mat4 model = view.get<TransformComponent>(entity).get_world_matrix();
		const Vec3 offset = Vec3(model[3]) - ctx.camera_position;
		draws.push_back({
			.material = material.material,
			.gpu_mesh = get_or_upload_mesh(mesh.data),
			.model = model,
			.material_index = material.material_index,
			.receives_shadows = static_cast<Uint32>(mesh.receive_shadows),
			.distance = glm::dot(offset, offset),
		});
	}
	if (draws.empty()) {
		return;
	}
	std::ranges::sort(draws, std::ranges::greater{}, &Draw::distance);
	if (animated) {
		Foundation::FrameScheduler::get()->request_frame();
	}

	auto *frame_data = ctx.frame_data;
	const Uint32 frame_slot = ctx.frame_slot;

	graph.add_pass(
		"Transparent",
		[&ctx](RG::RGPassBuilder &builder) {
			builder.read_buffer(ctx.h_light_list, RG::ResourceState::ShaderRead);
			builder.read_buffer(ctx.h_cluster_light_info, RG::ResourceState::ShaderRead);
			for (auto &shadow_handle : ctx.h_shadow_maps) {
				if (shadow_handle.is_valid()) {
					builder.read_texture(shadow_handle, RG::ResourceState::ShaderRead);
				}
			}
			if (ctx.h_local_shadow_atlas.is_valid()) {
				builder.read_texture(ctx.h_local_shadow_atlas, RG::ResourceState::ShaderRead);
			}
			ctx.h_scene_color = builder.set_color_attachment(0, ctx.h_scene_color, RG::AttachmentLoadOp::Load,
															 RG::AttachmentStoreOp::Store);
			ctx.h_depth =
				builder.set_depth_attachment(ctx.h_depth, RG::AttachmentLoadOp::Load, RG::AttachmentStoreOp::Store,
											 RG::AttachmentLoadOp::DontCare, RG::AttachmentStoreOp::DontCare, true);
		},
		[draws = std::move(draws), frame_data, frame_slot](GFX::GfxCommandList &cmd, RG::RGRegistry &) {
			Material *bound = nullptr;
			for (const Draw &draw : draws) {
				if (draw.material.get() != bound) {
					bound = draw.material.get();
					bound->flush(frame_slot);
					bound->bind(cmd, 1, frame_slot);
					cmd.bind_descriptor_set(0, frame_data->get_descriptor_set(frame_slot));
				}
				MeshPushConstants push{ .model = draw.model,
										.material_index = draw.material_index,
										.receives_shadows = draw.receives_shadows,
										.alpha_cutoff = -1.F,
										.transparent = 1 };
				cmd.push_constants(push, RHI::ShaderStageFlags::Vertex | RHI::ShaderStageFlags::Fragment);
				cmd.bind_vertex_buffer(draw.gpu_mesh->get_vertex_buffer());
				cmd.bind_index_buffer(draw.gpu_mesh->get_index_buffer());
				cmd.draw_indexed(draw.gpu_mesh->get_index_count());
			}
		});
}

} // namespace Aquila::Rendering
