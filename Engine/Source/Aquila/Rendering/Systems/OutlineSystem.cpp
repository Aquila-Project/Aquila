#include "Aquila/Rendering/Systems/OutlineSystem.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Scene/Visibility.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/OutlineComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"

namespace Aquila::Rendering {

using namespace Aquila::Graphics;
using namespace Aquila::SceneManagement::Components;

using Aquila::Foundation::EnginePaths::shaders_dir;

namespace {
std::string mask_shader_path() {
	return shaders_dir() + "OutlineMask.slang";
}

std::string outline_shader_path() {
	return shaders_dir() + "Outline.slang";
}

struct MaskPushConstants {
	Mat4 model;
};

struct OutlinePushConstants {
	Vec4 outline_color;
	Vec2 texel_size;
	F32 thickness;
	F32 padding;
};

bool collect_stages(const std::string &path, const char *label, RHI::GraphicsPipelineDesc &out) {
	std::vector<RHI::VulkanCompiledStage> stages;
	std::string err;

	if (!RHI::VulkanShaderCompiler::compile_file(path, stages, err)) {
		AQUILA_LOG_ERROR("{}: shader compile failed: {}", label, err);
		return false;
	}

	for (auto &stage : stages) {
		RHI::ShaderStageDesc shader_descriptor{ .spirv = stage.spirv, .entry_point = stage.entry_point_name };

		if (stage.stage == VK_SHADER_STAGE_VERTEX_BIT) {
			shader_descriptor.stage = RHI::ShaderStageFlags::Vertex;
			out.vertex_shader = shader_descriptor;
		} else if (stage.stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
			shader_descriptor.stage = RHI::ShaderStageFlags::Fragment;
			out.fragment_shader = shader_descriptor;
		}
	}

	return true;
}
} // namespace

void OutlineSystem::on_init(GFX::GfxContext &ctx) {
	RenderingSystemBase::on_init(ctx);

	m_outline_layout = ctx.create_descriptor_set_layout({
		.bindings = { {
			.binding = 0,
			.type = RHI::DescriptorType::CombinedImageSampler,
			.stages = RHI::ShaderStageFlags::Fragment,
			.count = 1,
		} },
	});

	for (auto &set : m_outline_sets) {
		set = ctx.allocate_descriptor_set(*m_outline_layout);
	}

	m_mask_pipeline = Shader::ReloadablePipeline::create(
		ctx, mask_shader_path(), [](GFX::GfxContext &build_ctx) -> Ref<GFX::GfxPipeline> {
			RHI::GraphicsPipelineDesc pipeline_descriptor{};

			if (!collect_stages(mask_shader_path(), "OutlineSystem (mask)", pipeline_descriptor)) {
				return nullptr;
			}

			pipeline_descriptor.topology = RHI::PrimitiveTopology::TriangleList;
			pipeline_descriptor.raster.cull_mode = RHI::CullMode::None;

			pipeline_descriptor.depth_stencil.depth_test = false;
			pipeline_descriptor.depth_stencil.depth_write = false;

			pipeline_descriptor.blend_attachments = { RHI::BlendAttachmentDesc{ .enable = false } };
			pipeline_descriptor.color_formats = { RHI::TextureFormat::R8 };
			pipeline_descriptor.depth_format = RHI::TextureFormat::None;

			pipeline_descriptor.set_layouts = { &SceneFrameData::get()->get_layout().get_rhi() };
			pipeline_descriptor.push_constants = { { .stages = RHI::ShaderStageFlags::Vertex,
													 .offset = 0,
													 .size = sizeof(MaskPushConstants) } };
			return build_ctx.create_graphics_pipeline(pipeline_descriptor);
		});

	m_outline_pipeline = Shader::ReloadablePipeline::create(
		ctx, outline_shader_path(), [this](GFX::GfxContext &build_ctx) -> Ref<GFX::GfxPipeline> {
			RHI::GraphicsPipelineDesc pipeline_descriptor{};

			if (!collect_stages(outline_shader_path(), "OutlineSystem (outline)", pipeline_descriptor)) {
				return nullptr;
			}

			pipeline_descriptor.topology = RHI::PrimitiveTopology::TriangleList;
			pipeline_descriptor.no_vertex_input = true;
			pipeline_descriptor.raster.cull_mode = RHI::CullMode::None;

			pipeline_descriptor.depth_stencil.depth_test = false;
			pipeline_descriptor.depth_stencil.depth_write = false;

			pipeline_descriptor.blend_attachments = { RHI::BlendAttachmentDesc{
				.enable = true,
				.src_color = RHI::BlendFactor::SrcAlpha,
				.dst_color = RHI::BlendFactor::OneMinusSrcAlpha,
				.color_op = RHI::BlendOp::Add,
				.src_alpha = RHI::BlendFactor::One,
				.dst_alpha = RHI::BlendFactor::OneMinusSrcAlpha,
				.alpha_op = RHI::BlendOp::Add,
			} };
			pipeline_descriptor.color_formats = { DISPLAY_COLOR_FORMAT };
			pipeline_descriptor.depth_format = RHI::TextureFormat::None;

			pipeline_descriptor.set_layouts = { &m_outline_layout->get_rhi() };
			pipeline_descriptor.push_constants = { { .stages = RHI::ShaderStageFlags::Fragment,
													 .offset = 0,
													 .size = sizeof(OutlinePushConstants) } };
			return build_ctx.create_graphics_pipeline(pipeline_descriptor);
		});
}

void OutlineSystem::add_passes(RG::RenderGraph &graph, FrameContext &ctx) {
	if (!m_mask_pipeline || !m_mask_pipeline->is_valid() || !m_outline_pipeline || !m_outline_pipeline->is_valid()) {
		return;
	}
	if (ctx.settings != nullptr && !ctx.settings->show_outline) {
		return;
	}

	struct DrawCall {
		Ref<GFX::GfxMesh> gpu_mesh;
		Mat4 model;
	};

	std::vector<DrawCall> draw_calls;
	Option<OutlineComponent> style;

	auto view = ctx.scene->get_registry().view<OutlineComponent, TransformComponent, MeshComponent>();
	for (auto entity : view) {
		if (!SceneManagement::is_visible_in_hierarchy(ctx.scene->get_registry(), entity)) {
			continue;
		}
		auto &mesh = view.get<MeshComponent>(entity);
		if (!mesh.is_valid()) {
			continue;
		}

		auto gpu_mesh = get_or_upload_mesh(mesh.data);
		if (!gpu_mesh) {
			continue;
		}

		if (!style) {
			style = view.get<OutlineComponent>(entity);
		}
		draw_calls.push_back({ .gpu_mesh = gpu_mesh, .model = view.get<TransformComponent>(entity).get_world_matrix() });
	}

	if (draw_calls.empty()) {
		return;
	}

	auto *frame_data = ctx.frame_data;
	const Uint32 frame_slot = ctx.frame_slot;

	RG::RGTextureHandle h_mask = graph.declare_texture({
		.width = ctx.width,
		.height = ctx.height,
		.format = RHI::TextureFormat::R8,
		.usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled,
		.debug_name = "OutlineMask",
	});

	graph.add_pass(
		"OutlineMask",

		[&h_mask](RG::RGPassBuilder &builder) {
			h_mask = builder.set_color_attachment(0, h_mask, RG::AttachmentLoadOp::Clear, RG::AttachmentStoreOp::Store,
												  { Foundation::Color::RGBA::BLACK });
		},

		[draw_calls = std::move(draw_calls), frame_data, frame_slot, this](GFX::GfxCommandList &cmd,
																		   RG::RGRegistry &) {
			cmd.bind_pipeline(m_mask_pipeline->get());
			cmd.bind_descriptor_set(0, frame_data->get_descriptor_set(frame_slot));

			for (const auto &draw : draw_calls) {
				MaskPushConstants push{ .model = draw.model };
				cmd.push_constants(push, RHI::ShaderStageFlags::Vertex);
				cmd.bind_vertex_buffer(draw.gpu_mesh->get_vertex_buffer());
				cmd.bind_index_buffer(draw.gpu_mesh->get_index_buffer());
				cmd.draw_indexed(draw.gpu_mesh->get_index_count());
			}
		});

	const RG::RGTextureHandle h_mask_read = h_mask;
	auto *set = m_outline_sets[frame_slot].get();

	const OutlinePushConstants outline_push{
		.outline_color = style->color,
		.texel_size = { 1.F / static_cast<F32>(ctx.width), 1.F / static_cast<F32>(ctx.height) },
		.thickness = style->thickness,
		.padding = 0.F,
	};

	graph.add_pass(
		"Outline",

		[h_mask_read, &ctx](RG::RGPassBuilder &builder) {
			builder.read_texture(h_mask_read, RG::ResourceState::ShaderRead);

			ctx.h_display_color = builder.set_color_attachment(0, ctx.h_display_color, RG::AttachmentLoadOp::Load,
															   RG::AttachmentStoreOp::Store);
		},

		[h_mask_read, set, outline_push, this](GFX::GfxCommandList &cmd, RG::RGRegistry &registry) {
			cmd.bind_pipeline(m_outline_pipeline->get());

			set->set_texture(0, registry.get_texture(h_mask_read));
			set->flush();
			cmd.bind_descriptor_set(0, *set);

			cmd.push_constants(outline_push, RHI::ShaderStageFlags::Fragment);
			cmd.draw(3);
		});
}

} // namespace Aquila::Rendering
