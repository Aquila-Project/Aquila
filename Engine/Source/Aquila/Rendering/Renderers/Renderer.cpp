#include "Aquila/Rendering/Renderers/Renderer.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxSwapchain.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"

using Aquila::Foundation::EnginePaths::shaders_dir;

namespace Aquila::Rendering {

void Renderer::on_init(GFX::GfxContext &ctx) {
	m_ctx = &ctx;

	// define blit layout
	m_blit_layout = ctx.create_descriptor_set_layout({
		.bindings = { {
			.binding = 0,
			.type = RHI::DescriptorType::CombinedImageSampler,
			.stages = RHI::ShaderStageFlags::Fragment,
			.count = 1,
		} },
	});

	m_blit_pipeline = Graphics::Shader::ReloadablePipeline::create(
		ctx, shaders_dir() + "Blit.slang", [this](GFX::GfxContext &build_ctx) -> Ref<GFX::GfxPipeline> {
			std::vector<RHI::VulkanCompiledStage> stages;
			std::string err;
			if (!RHI::VulkanShaderCompiler::compile_file(shaders_dir() + "Blit.slang", stages, err)) {
				AQUILA_LOG_ERROR("Renderer: blit shader compile failed: {}", err);
				return nullptr;
			}

			RHI::GraphicsPipelineDesc desc{};
			for (auto &stage : stages) {
				RHI::ShaderStageDesc shader_desc{ .spirv = stage.spirv, .entry_point = stage.entry_point_name };
				if (stage.stage == VK_SHADER_STAGE_VERTEX_BIT) {
					shader_desc.stage = RHI::ShaderStageFlags::Vertex;
					desc.vertex_shader = shader_desc;
				} else {
					shader_desc.stage = RHI::ShaderStageFlags::Fragment;
					desc.fragment_shader = shader_desc;
				}
			}
			desc.color_formats = { RHI::TextureFormat::BGRA8 };
			desc.depth_format = RHI::TextureFormat::None;
			desc.no_vertex_input = true;
			desc.raster.cull_mode = RHI::CullMode::None;
			desc.set_layouts = { &m_blit_layout->get_rhi() };
			return build_ctx.create_graphics_pipeline(desc);
		});

	for (auto &set : m_blit_sets) {
		set = ctx.allocate_descriptor_set(*m_blit_layout);
	}

	// create swapchain pass
	m_swapchain_pass = ctx.create_render_pass({ .color_attachments = { {
													.load_op = RHI::AttachmentLoadOp::DontCare,
													.store_op = RHI::AttachmentStoreOp::Store,
												} },
												.depth_attachment = {},
												.use_swapchain = true,
												.debug_name = "SwapchainBlit" });
}

void Renderer::on_shutdown() {
	for (auto &sys : m_systems) {
		sys->on_shutdown();
	}
}

void Renderer::on_resize(Uint32 width, Uint32 height) {
	for (auto &sys : m_systems) {
		sys->on_resize(width, height);
	}
}

void Renderer::set_swapchain_target(GFX::GfxSwapchain &swapchain, Uint32 image_index) {
	m_swapchain = &swapchain;
	m_swapchain_image_index = image_index;
	m_frame_slot = (m_frame_slot + 1) % SharedConstants::MAX_FRAMES_IN_FLIGHT;
}

void Renderer::add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) {
	for (auto &sys : m_systems) {
		sys->add_passes(graph, ctx);
	}
}

void Renderer::blit_to_swapchain(Graphics::RG::RenderGraph &graph, FrameContext &ctx) {
	if (ctx.swapchain_replaced || !m_blit_pipeline || !m_blit_pipeline->is_valid() || (m_swapchain == nullptr)) {
		return;
	}

	auto h_src = ctx.h_scene_color;
	auto *swapchain = m_swapchain;
	auto image_index = m_swapchain_image_index;
	auto *set = m_blit_sets[m_frame_slot].get();
	auto *pipeline = &m_blit_pipeline->get();
	auto *render_pass = m_swapchain_pass.get();

	graph.add_pass(
		"SwapchainBlit",
		[&](Graphics::RG::RGPassBuilder &builder) {
			builder.read_texture(h_src, Graphics::RG::ResourceState::ShaderRead);
			builder.mark_as_side_effect();
		},
		[h_src, swapchain, image_index, set, pipeline, render_pass](GFX::GfxCommandList &cmd,
																	Graphics::RG::RGRegistry &reg) {
			auto &src_tex = reg.get_texture(h_src);
			render_pass->begin(cmd, swapchain, image_index);
			cmd.bind_pipeline(*pipeline);
			set->set_texture(0, src_tex);
			set->flush();
			cmd.bind_descriptor_set(0, *set);
			cmd.draw(3);
			render_pass->end(cmd);
		});
}

} // namespace Aquila::Rendering
