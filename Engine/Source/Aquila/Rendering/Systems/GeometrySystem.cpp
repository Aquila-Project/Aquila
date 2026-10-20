#include "Aquila/Rendering/Systems/GeometrySystem.h"
#include "Aquila/Scene/Visibility.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/Components/MaterialComponent.h"
#include "Aquila/Scene/DefaultScenes.h"
#include "Aquila/Foundation/Color.h"
#include "Aquila/Foundation/FrameScheduler.h"

namespace Aquila::Rendering {

using namespace SceneManagement::Components;
using namespace Graphics;

struct MeshPushConstants {
	Mat4 model;
	Vec4 color = Vec4(1.F);
	Uint32 material_index = 0;
	Uint32 receives_shadows = 0;
};

void GeometrySystem::on_init(GFX::GfxContext &ctx) {
	RenderingSystemBase::on_init(ctx);
}

void GeometrySystem::add_passes(RG::RenderGraph &graph, FrameContext &ctx) {
	if (ctx.settings != nullptr && ctx.settings->debug_view != DebugView::Lit) {
		return;
	}
	auto &registry = ctx.scene->get_registry();
	auto view = registry.view<TransformComponent, MeshComponent>();

	struct DrawCall {
		Ref<GFX::GfxMesh> gpu_mesh;
		Mat4 model;
		Uint32 material_index = 0;
		Uint32 receives_shadows = 0;
	};

	struct Batch {
		Ref<Material> material;
		std::vector<DrawCall> draws;
	};
	std::unordered_map<Material *, Batch> batches;
	bool animated = false;

	for (auto entity : view) {
		if (!SceneManagement::is_visible_in_hierarchy(registry, entity)) {
			continue;
		}
		auto &transform = view.get<TransformComponent>(entity);
		auto &mesh = view.get<MeshComponent>(entity);
		if (!mesh.is_valid()) {
			continue;
		}

		auto *mat = registry.try_get<MaterialComponent>(entity);
		if (mat == nullptr) {
			continue;
		}
		SceneManagement::ensure_material(*mat, *m_ctx);
		if (!mat->material || mat->material->get_domain() == Shader::ShaderDomain::Fullscreen) {
			continue;
		}

		animated = animated || mat->material->uses_time();
		Batch &batch = batches[mat->material.get()];
		batch.material = mat->material;
		batch.draws.push_back({ .gpu_mesh = get_or_upload_mesh(mesh.data),
								.model = transform.get_world_matrix(),
								.material_index = mat->material_index,
								.receives_shadows = static_cast<Uint32>(mesh.receive_shadows) });
	}

	if (animated) {
		Foundation::FrameScheduler::get()->request_frame();
	}

	auto *frame_data = ctx.frame_data;
	const Uint32 frame_slot = ctx.frame_slot;

	graph.add_pass(
		"Geometry",
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

			ctx.h_scene_color =
				builder.set_color_attachment(0, ctx.h_scene_color, RG::AttachmentLoadOp::Clear,
											 RG::AttachmentStoreOp::Store, { Foundation::Color::RGBA::DARK_GRAY });

			ctx.h_depth =
				builder.set_depth_attachment(ctx.h_depth, RG::AttachmentLoadOp::Clear, RG::AttachmentStoreOp::Store,
											 RG::AttachmentLoadOp::DontCare, RG::AttachmentStoreOp::DontCare,
											 /*read_only=*/false, RG::ClearDepth{ .depth = 1.F });
		},
		[batches = std::move(batches), frame_data, frame_slot](GFX::GfxCommandList &cmd, RG::RGRegistry &) {
			for (const auto &[key, batch] : batches) {
				Material *material = batch.material.get();
				material->flush(frame_slot);
				material->bind(cmd, 1, frame_slot);
				cmd.bind_descriptor_set(0, frame_data->get_descriptor_set(frame_slot));

				for (const auto &dc : batch.draws) {
					MeshPushConstants push{ .model = dc.model,
											.material_index = dc.material_index,
											.receives_shadows = dc.receives_shadows };
					cmd.push_constants(push, RHI::ShaderStageFlags::Vertex | RHI::ShaderStageFlags::Fragment);
					cmd.bind_vertex_buffer(dc.gpu_mesh->get_vertex_buffer());
					cmd.bind_index_buffer(dc.gpu_mesh->get_index_buffer());
					cmd.draw_indexed(dc.gpu_mesh->get_index_count());
				}
			}
		});
}

} // namespace Aquila::Rendering
