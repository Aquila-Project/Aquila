#include "Aquila/Rendering/Systems/BloomSystem.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Graphics/Material/MaterialFactory.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Rendering/MeshPushConstants.h"
#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/Scene/Components/MaterialComponent.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scene/Visibility.h"

namespace Aquila::Rendering {

using namespace Graphics;
using namespace SceneManagement::Components;

namespace {

struct EmissiveDraw {
	Ref<GFX::GfxMesh> mesh;
	Mat4 model;
	Uint32 material_index = 0;
};

struct EmissiveBatch {
	Ref<Material> material;
	GFX::GfxPipeline *pipeline = nullptr;
	F32 alpha_cutoff = -1.F;
	std::vector<EmissiveDraw> draws;
};

struct BloomFinalPushConstants {
	F32 scene_weight = 1.F;
	F32 bloom_weight = 0.F;
};

Ref<Shader::ReloadablePipeline> create_pipeline(GFX::GfxContext &ctx, const std::string &file,
												Ref<GFX::GfxDescriptorSetLayout> layout, bool additive,
												Uint32 push_size) {
	const std::string path = Foundation::EnginePaths::shaders_dir() + file;
	return Shader::ReloadablePipeline::create(
		ctx, path, [layout, path, additive, push_size](GFX::GfxContext &build_ctx) -> Ref<GFX::GfxPipeline> {
			std::vector<RHI::VulkanCompiledStage> stages;
			std::string err;
			if (!RHI::VulkanShaderCompiler::compile_file(path, stages, err)) {
				AQUILA_LOG_ERROR("BloomSystem: shader compile failed: {}", err);
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
			desc.color_formats = { SCENE_COLOR_FORMAT };
			desc.depth_format = RHI::TextureFormat::None;
			desc.no_vertex_input = true;
			desc.raster.cull_mode = RHI::CullMode::None;
			desc.set_layouts = { &layout->get_rhi() };
			if (additive) {
				desc.blend_attachments = { {
					.enable = true,
					.src_color = RHI::BlendFactor::One,
					.dst_color = RHI::BlendFactor::One,
					.src_alpha = RHI::BlendFactor::Zero,
					.dst_alpha = RHI::BlendFactor::One,
				} };
			}
			if (push_size > 0) {
				desc.push_constants = { { .stages = RHI::ShaderStageFlags::Fragment, .offset = 0, .size = push_size } };
			}
			return build_ctx.create_graphics_pipeline(desc);
		});
}

RHI::DescriptorBinding sampled_binding(Uint32 binding) {
	return {
		.binding = binding,
		.type = RHI::DescriptorType::CombinedImageSampler,
		.stages = RHI::ShaderStageFlags::Fragment,
		.count = 1,
	};
}

} // namespace

void BloomSystem::on_init(GFX::GfxContext &ctx) {
	RenderingSystemBase::on_init(ctx);

	m_layout = ctx.create_descriptor_set_layout({ .bindings = { sampled_binding(0) } });
	m_final_layout = ctx.create_descriptor_set_layout({ .bindings = { sampled_binding(0), sampled_binding(1) } });
	for (auto &frame_sets : m_downsample_sets) {
		for (auto &set : frame_sets) {
			set = ctx.allocate_descriptor_set(*m_layout);
		}
	}
	for (auto &frame_sets : m_upsample_sets) {
		for (auto &set : frame_sets) {
			set = ctx.allocate_descriptor_set(*m_layout);
		}
	}
	for (auto &set : m_final_sets) {
		set = ctx.allocate_descriptor_set(*m_final_layout);
	}

	m_downsample_pipeline = create_pipeline(ctx, "BloomDownsample.slang", m_layout, false, 0);
	m_upsample_pipeline = create_pipeline(ctx, "BloomUpsample.slang", m_layout, true, 0);
	m_final_pipeline = create_pipeline(ctx, "BloomFinal.slang", m_final_layout, false, sizeof(BloomFinalPushConstants));
}

bool BloomSystem::pipelines_ready() const {
	return m_downsample_pipeline && m_downsample_pipeline->is_valid() && m_upsample_pipeline &&
		m_upsample_pipeline->is_valid() && m_final_pipeline && m_final_pipeline->is_valid();
}

void BloomSystem::ensure_targets(Uint32 width, Uint32 height) {
	if (m_final && width == m_width && height == m_height) {
		return;
	}
	m_width = width;
	m_height = height;

	m_emissive = m_ctx->create_texture({
		.width = width,
		.height = height,
		.format = SCENE_COLOR_FORMAT,
		.usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled,
		.sampler = RHI::SamplerDesc::render_target(),
		.debug_name = "BloomEmissive",
	});

	Uint32 mip_width = width;
	Uint32 mip_height = height;
	for (Uint32 i = 0; i < k_mip_count; ++i) {
		mip_width = std::max(1U, mip_width / 2);
		mip_height = std::max(1U, mip_height / 2);
		m_mips[i] = m_ctx->create_texture({
			.width = mip_width,
			.height = mip_height,
			.format = SCENE_COLOR_FORMAT,
			.usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled,
			.sampler = RHI::SamplerDesc::render_target(),
			.debug_name = "BloomMip" + std::to_string(i),
		});
	}

	m_final = m_ctx->create_texture({
		.width = width,
		.height = height,
		.format = SCENE_COLOR_FORMAT,
		.usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled,
		.sampler = RHI::SamplerDesc::render_target(),
		.debug_name = "SceneColorBloom",
	});
}

void BloomSystem::add_passes(RG::RenderGraph &graph, FrameContext &ctx) {
	const bool lit = ctx.settings == nullptr || ctx.settings->debug_view == DebugView::Lit;
	if (!ctx.bloom || ctx.bloom_intensity <= 0.F || !lit || ctx.scene == nullptr || ctx.width == 0 || ctx.height == 0 ||
		!pipelines_ready()) {
		return;
	}

	auto &registry = ctx.scene->get_registry();
	std::unordered_map<Material *, EmissiveBatch> batches;
	for (auto entity : registry.view<TransformComponent, MeshComponent, MaterialComponent>()) {
		if (!SceneManagement::is_visible_in_hierarchy(registry, entity)) {
			continue;
		}
		const auto &mesh = registry.get<MeshComponent>(entity);
		const auto &material_component = registry.get<MaterialComponent>(entity);
		const Ref<Material> &material = material_component.material;
		if (!mesh.is_valid() || !material || material->get_blend_mode() == BlendMode::Transparent) {
			continue;
		}
		if (const Option<Vec3> emission = material->get_average_emission();
			emission && Math::max(emission->x, Math::max(emission->y, emission->z)) <= 0.F) {
			continue;
		}
		GFX::GfxPipeline *pipeline = MaterialFactory::get()->get_emissive_pipeline(*m_ctx, *material);
		if (pipeline == nullptr) {
			continue;
		}

		EmissiveBatch &batch = batches[material.get()];
		batch.material = material;
		batch.pipeline = pipeline;
		batch.alpha_cutoff = material->get_blend_mode() == BlendMode::Masked ? material->get_data().alpha_cutoff : -1.F;
		batch.draws.push_back({
			.mesh = get_or_upload_mesh(mesh.data),
			.model = registry.get<TransformComponent>(entity).get_world_matrix(),
			.material_index = material_component.material_index,
		});
	}
	if (batches.empty()) {
		return;
	}

	ensure_targets(ctx.width, ctx.height);

	RG::RGTextureHandle h_emissive = graph.import_texture(m_emissive.get(), m_emissive->get_desc().debug_name);
	const RG::RGTextureHandle h_depth = ctx.h_depth;
	auto *frame_data = ctx.frame_data;
	const Uint32 frame_slot = ctx.frame_slot;

	graph.add_pass(
		"Bloom - Emissive",
		[&h_emissive, h_depth](RG::RGPassBuilder &builder) {
			h_emissive = builder.set_color_attachment(0, h_emissive, RG::AttachmentLoadOp::Clear,
													  RG::AttachmentStoreOp::Store, RG::ClearColor{ Vec4(0.F) });
			builder.set_depth_attachment(h_depth, RG::AttachmentLoadOp::Load, RG::AttachmentStoreOp::Store,
										 RG::AttachmentLoadOp::DontCare, RG::AttachmentStoreOp::DontCare,
										 /*read_only=*/true);
		},
		[batches = std::move(batches), frame_data, frame_slot](GFX::GfxCommandList &cmd, RG::RGRegistry &) {
			for (const auto &[key, batch] : batches) {
				batch.material->flush(frame_slot);
				cmd.bind_pipeline(*batch.pipeline);
				cmd.bind_descriptor_set(0, frame_data->get_descriptor_set(frame_slot));
				batch.material->bind_descriptor_set(cmd, 1, frame_slot);

				for (const auto &draw : batch.draws) {
					const MeshPushConstants push{ .model = draw.model,
												  .material_index = draw.material_index,
												  .alpha_cutoff = batch.alpha_cutoff };
					cmd.push_constants(push, RHI::ShaderStageFlags::Vertex | RHI::ShaderStageFlags::Fragment);
					cmd.bind_vertex_buffer(draw.mesh->get_vertex_buffer());
					cmd.bind_index_buffer(draw.mesh->get_index_buffer());
					cmd.draw_indexed(draw.mesh->get_index_count());
				}
			}
		});

	std::array<RG::RGTextureHandle, k_mip_count> mips;
	for (Uint32 i = 0; i < k_mip_count; ++i) {
		mips[i] = graph.import_texture(m_mips[i].get(), m_mips[i]->get_desc().debug_name);
	}

	auto *downsample_pipeline = m_downsample_pipeline.get();
	for (Uint32 i = 0; i < k_mip_count; ++i) {
		const RG::RGTextureHandle h_source = i == 0 ? h_emissive : mips[i - 1];
		auto *set = m_downsample_sets[ctx.frame_slot][i].get();

		graph.add_pass(
			"Bloom - Downsample " + std::to_string(i),
			[h_source, &mip = mips[i]](RG::RGPassBuilder &builder) {
				builder.read_texture(h_source, RG::ResourceState::ShaderRead);
				mip =
					builder.set_color_attachment(0, mip, RG::AttachmentLoadOp::DontCare, RG::AttachmentStoreOp::Store);
			},
			[set, downsample_pipeline, h_source](GFX::GfxCommandList &cmd, RG::RGRegistry &registry) {
				set->set_texture(0, registry.get_texture(h_source)).flush();
				cmd.bind_pipeline(downsample_pipeline->get());
				cmd.bind_descriptor_set(0, *set);
				cmd.draw(3);
			});
	}

	auto *upsample_pipeline = m_upsample_pipeline.get();
	for (Int32 level = static_cast<Int32>(k_mip_count) - 2; level >= 0; --level) {
		const RG::RGTextureHandle h_source = mips[level + 1];
		auto *set = m_upsample_sets[ctx.frame_slot][level].get();

		graph.add_pass(
			"Bloom - Upsample " + std::to_string(level),
			[h_source, &mip = mips[level]](RG::RGPassBuilder &builder) {
				builder.read_texture(h_source, RG::ResourceState::ShaderRead);
				mip = builder.set_color_attachment(0, mip, RG::AttachmentLoadOp::Load, RG::AttachmentStoreOp::Store);
			},
			[set, upsample_pipeline, h_source](GFX::GfxCommandList &cmd, RG::RGRegistry &registry) {
				set->set_texture(0, registry.get_texture(h_source)).flush();
				cmd.bind_pipeline(upsample_pipeline->get());
				cmd.bind_descriptor_set(0, *set);
				cmd.draw(3);
			});
	}

	const BloomFinalPushConstants push{
		.scene_weight = 1.F,
		.bloom_weight = ctx.bloom_intensity / static_cast<F32>(k_mip_count),
	};
	const RG::RGTextureHandle h_scene = ctx.h_scene_color;
	const RG::RGTextureHandle h_bloom = mips[0];
	RG::RGTextureHandle h_final = graph.import_texture(m_final.get(), m_final->get_desc().debug_name);
	auto *set = m_final_sets[ctx.frame_slot].get();
	auto *final_pipeline = m_final_pipeline.get();

	graph.add_pass(
		"Bloom - Composite",
		[h_scene, h_bloom, &h_final](RG::RGPassBuilder &builder) {
			builder.read_texture(h_scene, RG::ResourceState::ShaderRead);
			builder.read_texture(h_bloom, RG::ResourceState::ShaderRead);
			h_final =
				builder.set_color_attachment(0, h_final, RG::AttachmentLoadOp::DontCare, RG::AttachmentStoreOp::Store);
		},
		[set, final_pipeline, h_scene, h_bloom, push](GFX::GfxCommandList &cmd, RG::RGRegistry &registry) {
			set->set_texture(0, registry.get_texture(h_scene)).set_texture(1, registry.get_texture(h_bloom)).flush();
			cmd.bind_pipeline(final_pipeline->get());
			cmd.bind_descriptor_set(0, *set);
			cmd.push_constants(push, RHI::ShaderStageFlags::Fragment);
			cmd.draw(3);
		});

	ctx.h_bloom_final = h_final;
}

} // namespace Aquila::Rendering
