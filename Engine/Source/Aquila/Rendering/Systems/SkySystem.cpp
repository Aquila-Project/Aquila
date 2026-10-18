#include "Aquila/Rendering/Systems/SkySystem.h"
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
#include "Aquila/Scene/Components/SkyLightComponent.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Foundation/Math/Math.h"

namespace Aquila::Rendering {

using namespace Graphics;
using namespace SceneManagement::Components;
using Aquila::Foundation::EnginePaths::shaders_dir;

void SkySystem::on_init(GFX::GfxContext &ctx) {
	RenderingSystemBase::on_init(ctx);

	m_sky_layout = ctx.create_descriptor_set_layout({
		.bindings = { {
			.binding = 0,
			.type = RHI::DescriptorType::UniformBuffer,
			.stages = RHI::ShaderStageFlags::Fragment,
			.count = 1,
		} },
	});

	for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
		m_sky_buffers[i] = ctx.create_buffer({
			.size = sizeof(GpuSkyData),
			.usage = RHI::BufferUsage::UniformBuffer,
			.domain = RHI::MemoryDomain::CpuToGpu,
			.debug_name = "SkyData_" + std::to_string(i),
		});
		m_sky_sets[i] = ctx.allocate_descriptor_set(*m_sky_layout);
		m_sky_sets[i]->set_buffer(0, *m_sky_buffers[i]).flush();
	}

	m_pipeline = Shader::ReloadablePipeline::create(
		ctx, shaders_dir() + "HosekWilkieSky.slang", [this](GFX::GfxContext &build_ctx) -> Ref<GFX::GfxPipeline> {
			std::vector<RHI::VulkanCompiledStage> stages;
			std::string err;
			if (!RHI::VulkanShaderCompiler::compile_file(shaders_dir() + "HosekWilkieSky.slang", stages, err)) {
				AQUILA_LOG_ERROR("SkySystem: shader compile failed: {}", err);
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
			pipeline_descriptor.set_layouts = { &SceneFrameData::get()->get_layout().get_rhi(),
												&m_sky_layout->get_rhi() };
			return build_ctx.create_graphics_pipeline(pipeline_descriptor);
		});
}

void SkySystem::add_passes(RG::RenderGraph &graph, FrameContext &ctx) {
	if (!m_pipeline || !m_pipeline->is_valid() || ctx.scene == nullptr) {
		return;
	}
	if (ctx.settings != nullptr &&
		(!ctx.settings->show_sky ||
		 (ctx.settings->debug_view != DebugView::Lit && ctx.settings->debug_view != DebugView::Albedo))) {
		return;
	}

	auto &registry = ctx.scene->get_registry();
	SkyLightComponent *sky = nullptr;
	for (auto entity : registry.view<SkyLightComponent>()) {
		auto &candidate = registry.get<SkyLightComponent>(entity);
		if (candidate.is_active() && candidate.get_source() == SkySource::Procedural &&
			SceneManagement::is_visible_in_hierarchy(registry, entity)) {
			sky = &candidate;
			break;
		}
	}

	if (sky == nullptr) {
		return;
	}

	if (sky->is_dirty()) {
		m_cached = HosekWilkieSky::build(Math::radians(sky->get_sun_elevation()), sky->get_turbidity(),
										 sky->get_ground_albedo(), sky->get_sun_direction());
		sky->set_irradiance(HosekWilkieSky::bake_sh(m_cached));
	}

	m_cached.tint_intensity = Vec4(sky->get_tint(), sky->get_intensity() * kSkyBaseExposure);

	const Uint32 frame_slot = ctx.frame_slot;
	m_sky_buffers[frame_slot]->write(&m_cached, sizeof(GpuSkyData));

	graph.add_pass(
		"Sky",
		[&ctx](RG::RGPassBuilder &builder) {
			ctx.h_scene_color = builder.set_color_attachment(0, ctx.h_scene_color, RG::AttachmentLoadOp::Load,
															 RG::AttachmentStoreOp::Store);
			ctx.h_depth = builder.set_depth_attachment(
				ctx.h_depth, RG::AttachmentLoadOp::Load, RG::AttachmentStoreOp::Store, RG::AttachmentLoadOp::DontCare,
				RG::AttachmentStoreOp::DontCare, true);
		},
		[this, frame_slot](GFX::GfxCommandList &cmd, RG::RGRegistry &) {
			cmd.bind_pipeline(m_pipeline->get());
			cmd.bind_descriptor_set(0, SceneFrameData::get()->get_descriptor_set(frame_slot));
			cmd.bind_descriptor_set(1, *m_sky_sets[frame_slot]);
			cmd.draw(3);
		});
}

} // namespace Aquila::Rendering
