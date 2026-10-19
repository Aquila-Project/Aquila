#include "Aquila/Rendering/Systems/ToneMapSystem.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"
#include "Aquila/Rendering/FrameContext.h"

namespace Aquila::Rendering {

using namespace Graphics;
using Aquila::Foundation::EnginePaths::shaders_dir;

namespace {

struct ToneMapPushConstants {
	Uint32 tone_mapper = 0;
	Uint32 display_raw = 0;
};

} // namespace

void ToneMapSystem::on_init(GFX::GfxContext &ctx) {
	RenderingSystemBase::on_init(ctx);

	m_layout = ctx.create_descriptor_set_layout({
		.bindings = { {
			.binding = 0,
			.type = RHI::DescriptorType::CombinedImageSampler,
			.stages = RHI::ShaderStageFlags::Fragment,
			.count = 1,
		}, },
	});
	for (auto &set : m_sets) {
		set = ctx.allocate_descriptor_set(*m_layout);
	}

	const std::string path = shaders_dir() + "ToneMap.slang";
	m_pipeline = Shader::ReloadablePipeline::create(
		ctx, path, [this, path](GFX::GfxContext &build_ctx) -> Ref<GFX::GfxPipeline> {
			std::vector<RHI::VulkanCompiledStage> stages;
			std::string err;
			if (!RHI::VulkanShaderCompiler::compile_file(path, stages, err)) {
				AQUILA_LOG_ERROR("ToneMapSystem: shader compile failed: {}", err);
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
			desc.color_formats = { DISPLAY_COLOR_FORMAT };
			desc.depth_format = RHI::TextureFormat::None;
			desc.no_vertex_input = true;
			desc.raster.cull_mode = RHI::CullMode::None;
			desc.set_layouts = { &m_layout->get_rhi() };
			desc.push_constants = {
				{ .stages = RHI::ShaderStageFlags::Fragment, .offset = 0, .size = sizeof(ToneMapPushConstants) }
			};
			return build_ctx.create_graphics_pipeline(desc);
		});
}

void ToneMapSystem::add_passes(RG::RenderGraph &graph, FrameContext &ctx) {
	if (!m_pipeline || !m_pipeline->is_valid()) {
		return;
	}

	const RenderSettings defaults;
	const RenderSettings &settings = ctx.settings != nullptr ? *ctx.settings : defaults;
	const ToneMapPushConstants push{
		.tone_mapper = static_cast<Uint32>(settings.tone_mapper),
		.display_raw = settings.debug_view == DebugView::Lit ? 0U : 1U,
	};
	const RG::RGTextureHandle h_source = ctx.h_scene_color;
	auto *set = m_sets[ctx.frame_slot].get();
	auto *pipeline = m_pipeline.get();

	graph.add_pass(
		"ToneMap",
		[&ctx, h_source](RG::RGPassBuilder &builder) {
			builder.read_texture(h_source, RG::ResourceState::ShaderRead);
			ctx.h_display_color = builder.set_color_attachment(0, ctx.h_display_color, RG::AttachmentLoadOp::DontCare,
															   RG::AttachmentStoreOp::Store);
		},
		[h_source, set, pipeline, push](GFX::GfxCommandList &cmd, RG::RGRegistry &registry) {
			set->set_texture(0, registry.get_texture(h_source));
			set->flush();
			cmd.bind_pipeline(pipeline->get());
			cmd.bind_descriptor_set(0, *set);
			cmd.push_constants(push, RHI::ShaderStageFlags::Fragment);
			cmd.draw(3);
		});
}

} // namespace Aquila::Rendering
