#include "Aquila/Rendering/Systems/FullscreenEffectSystem.h"
#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Graphics/Material/MaterialAssets.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/Scene/Components/FullscreenEffectComponent.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scene/Visibility.h"

namespace Aquila::Rendering {

using namespace Graphics;
using SceneManagement::Components::FullscreenEffectComponent;

void FullscreenEffectSystem::add_passes(RG::RenderGraph &graph, FrameContext &ctx) {
	if (ctx.scene == nullptr || (ctx.settings != nullptr && ctx.settings->debug_view != DebugView::Lit)) {
		return;
	}

	auto &registry = ctx.scene->get_registry();
	std::vector<const FullscreenEffectComponent *> effects;
	for (auto entity : registry.view<FullscreenEffectComponent>()) {
		const auto &effect = registry.get<FullscreenEffectComponent>(entity);
		if (effect.active && !effect.material.empty() && SceneManagement::is_visible_in_hierarchy(registry, entity)) {
			effects.push_back(&effect);
		}
	}
	if (effects.empty()) {
		return;
	}
	std::ranges::stable_sort(effects, {}, &FullscreenEffectComponent::order);

	auto *frame_data = ctx.frame_data;
	const Uint32 frame_slot = ctx.frame_slot;
	std::set<const Material *> used;

	for (const FullscreenEffectComponent *effect : effects) {
		Ref<Material> material = MaterialAssets::get()->load(*m_ctx, effect->material);
		if (!material || material->get_domain() != Shader::ShaderDomain::Fullscreen || used.contains(material.get())) {
			continue;
		}
		used.insert(material.get());

		const MaterialParameter *color_input = material->get_parameter("sceneColor");
		const MaterialParameter *depth_input = material->get_parameter("sceneDepth");
		const Uint32 color_binding = color_input != nullptr ? color_input->texture_binding : UINT32_MAX;
		const Uint32 depth_binding = depth_input != nullptr ? depth_input->texture_binding : UINT32_MAX;
		if (material->uses_time()) {
			Foundation::FrameScheduler::get()->request_frame();
		}

		const RG::RGTextureHandle input = ctx.h_scene_color;
		const RG::RGTextureHandle depth = ctx.h_depth;
		RG::RGTextureHandle output = graph.declare_texture({
			.width = ctx.width,
			.height = ctx.height,
			.format = SCENE_COLOR_FORMAT,
			.usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled,
			.debug_name = "FullscreenEffect",
		});

		graph.add_pass(
			"FullscreenEffect " + material->name,
			[input, depth, &output](RG::RGPassBuilder &builder) {
				builder.read_texture(input, RG::ResourceState::ShaderRead);
				builder.read_texture(depth, RG::ResourceState::ShaderRead);
				output = builder.set_color_attachment(0, output, RG::AttachmentLoadOp::DontCare,
													  RG::AttachmentStoreOp::Store);
			},
			[material, input, depth, color_binding, depth_binding, frame_data,
			 frame_slot](GFX::GfxCommandList &cmd, RG::RGRegistry &resources) {
				material->flush(frame_slot);
				if (color_binding != UINT32_MAX) {
					material->bind_input(frame_slot, color_binding, resources.get_texture(input));
				}
				if (depth_binding != UINT32_MAX) {
					material->bind_input(frame_slot, depth_binding, resources.get_texture(depth));
				}
				material->bind(cmd, 1, frame_slot);
				cmd.bind_descriptor_set(0, frame_data->get_descriptor_set(frame_slot));
				cmd.draw(3);
			});

		ctx.h_scene_color = output;
	}
}

}
