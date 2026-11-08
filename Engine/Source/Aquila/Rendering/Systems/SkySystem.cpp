#include "Aquila/Rendering/Systems/SkySystem.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/JobSystem.h"
#include "Aquila/Scene/Visibility.h"
#include "Aquila/Rendering/Environment/EnvironmentImage.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"
#include "Aquila/Graphics/Texture/MipChain.h"
#include "Aquila/Graphics/Texture/TextureLoader.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scene/Components/SkyLightComponent.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/Photometry.h"

namespace Aquila::Rendering {

using namespace Graphics;
using namespace SceneManagement::Components;
using Aquila::Foundation::EnginePaths::shaders_dir;

namespace {

constexpr Uint32 K_CUBE_FACES = 6;

struct SkyCubePush {
	Uint32 face = 0;
	Uint32 image = 0;
	F32 image_lod = 0.F;
};

struct PrefilterPush {
	Uint32 face = 0;
	F32 roughness = 0.F;
	F32 source_size = 0.F;
	F32 base_lod = 0.F;
};

bool compile_fullscreen_stages(const std::string &file, RHI::GraphicsPipelineDesc &desc) {
	std::vector<RHI::VulkanCompiledStage> stages;
	std::string err;
	if (!RHI::VulkanShaderCompiler::compile_file(shaders_dir() + file, stages, err)) {
		AQUILA_LOG_ERROR("SkySystem: {} failed to compile: {}", file, err);
		return false;
	}
	for (auto &stage : stages) {
		RHI::ShaderStageDesc shader_descriptor{ .spirv = stage.spirv, .entry_point = stage.entry_point_name };
		if (stage.stage == VK_SHADER_STAGE_VERTEX_BIT) {
			shader_descriptor.stage = RHI::ShaderStageFlags::Vertex;
			desc.vertex_shader = shader_descriptor;
		} else {
			shader_descriptor.stage = RHI::ShaderStageFlags::Fragment;
			desc.fragment_shader = shader_descriptor;
		}
	}
	desc.no_vertex_input = true;
	desc.topology = RHI::PrimitiveTopology::TriangleList;
	desc.raster.cull_mode = RHI::CullMode::None;
	return true;
}

Ref<Shader::ReloadablePipeline> create_sky_pipeline(GFX::GfxContext &ctx, const std::string &file,
													GFX::GfxDescriptorSetLayout &sky_layout) {
	return Shader::ReloadablePipeline::create(
		ctx, shaders_dir() + file, [file, &sky_layout](GFX::GfxContext &build_ctx) -> Ref<GFX::GfxPipeline> {
			RHI::GraphicsPipelineDesc pipeline_descriptor{};
			if (!compile_fullscreen_stages(file, pipeline_descriptor)) {
				return nullptr;
			}

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
			pipeline_descriptor.set_layouts = { &SceneFrameData::get()->get_layout().get_rhi(), &sky_layout.get_rhi() };
			pipeline_descriptor.debug_name = file;
			return build_ctx.create_graphics_pipeline(pipeline_descriptor);
		});
}

}

struct SkySystem::ImageInbox {
	struct Finished {
		Uint64 request = 0;
		EnvironmentImage image;
	};

	std::mutex mutex;
	std::vector<Finished> done;
};

SkySystem::SkySystem() : m_inbox(std::make_shared<ImageInbox>()) {}

SkySystem::~SkySystem() = default;

void SkySystem::on_init(GFX::GfxContext &ctx) {
	RenderingSystemBase::on_init(ctx);

	m_sky_layout = ctx.create_descriptor_set_layout({
		.bindings = {
			{
				.binding = 0,
				.type = RHI::DescriptorType::UniformBuffer,
				.stages = RHI::ShaderStageFlags::Fragment,
				.count = 1,
			},
			{
				.binding = 1,
				.type = RHI::DescriptorType::CombinedImageSampler,
				.stages = RHI::ShaderStageFlags::Fragment,
				.count = 1,
			},
		},
	});
	m_cube_layout = ctx.create_descriptor_set_layout({
		.bindings = { {
			.binding = 0,
			.type = RHI::DescriptorType::CombinedImageSampler,
			.stages = RHI::ShaderStageFlags::Fragment,
			.count = 1,
		} },
	});

	const std::array<Uint8, 4> black = { 0, 0, 0, 255 };
	m_placeholder = Texture::upload_mip_chain(
		ctx, Texture::MipChain{ .width = 1, .height = 1, .levels = { { black.begin(), black.end() } } },
		RHI::TextureFormat::RGBA8, RHI::SamplerDesc::texture2_d(), "SkyImagePlaceholder");

	for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
		m_sky_buffers[i] = ctx.create_buffer({
			.size = sizeof(GpuSkyData),
			.usage = RHI::BufferUsage::UniformBuffer,
			.domain = RHI::MemoryDomain::CpuToGpu,
			.debug_name = "SkyData_" + std::to_string(i),
		});
		m_sky_sets[i] = ctx.allocate_descriptor_set(*m_sky_layout);
		m_sky_sets[i]->set_buffer(0, *m_sky_buffers[i]).set_texture(1, *m_placeholder).flush();
		m_bound_images[i] = m_placeholder->get_id();
	}

	RHI::SamplerDesc source_sampler = RHI::SamplerDesc::render_target();
	source_sampler.max_lod = static_cast<F32>(K_SOURCE_MIPS - 1);
	m_source_cube = ctx.create_texture({
		.width = K_SOURCE_SIZE,
		.height = K_SOURCE_SIZE,
		.mip_levels = K_SOURCE_MIPS,
		.array_layers = K_CUBE_FACES,
		.format = RHI::TextureFormat::RGBA16F,
		.usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferSrc,
		.view_type = RHI::TextureViewType::Cube,
		.sampler = source_sampler,
		.debug_name = "SkySourceCube",
	});
	ctx.execute_immediate(RHI::CommandListType::Graphics, [this](GFX::GfxCommandList &cmd) {
		cmd.transition_texture(*m_source_cube, RHI::ResourceState::Undefined, RHI::ResourceState::ShaderRead);
	});
	m_cube_set = ctx.allocate_descriptor_set(*m_cube_layout);
	m_cube_set->set_texture(0, *m_source_cube).flush();

	m_pipeline = create_sky_pipeline(ctx, "HosekWilkieSky.slang", *m_sky_layout);
	m_image_pipeline = create_sky_pipeline(ctx, "SkyImage.slang", *m_sky_layout);

	m_cube_pipeline = Shader::ReloadablePipeline::create(
		ctx, shaders_dir() + "SkyCube.slang", [this](GFX::GfxContext &build_ctx) -> Ref<GFX::GfxPipeline> {
			RHI::GraphicsPipelineDesc pipeline_descriptor{};
			if (!compile_fullscreen_stages("SkyCube.slang", pipeline_descriptor)) {
				return nullptr;
			}

			pipeline_descriptor.depth_stencil.depth_test = false;
			pipeline_descriptor.depth_stencil.depth_write = false;
			pipeline_descriptor.color_formats = { RHI::TextureFormat::RGBA16F };
			pipeline_descriptor.depth_format = RHI::TextureFormat::None;
			pipeline_descriptor.set_layouts = { &SceneFrameData::get()->get_layout().get_rhi(),
												&m_sky_layout->get_rhi() };
			pipeline_descriptor.push_constants = { {
				.stages = RHI::ShaderStageFlags::Fragment,
				.offset = 0,
				.size = sizeof(SkyCubePush),
			} };
			pipeline_descriptor.debug_name = "SkyCube";
			m_prefiltered = false;
			return build_ctx.create_graphics_pipeline(pipeline_descriptor);
		});

	m_prefilter_pipeline = Shader::ReloadablePipeline::create(
		ctx, shaders_dir() + "SkyPrefilter.slang", [this](GFX::GfxContext &build_ctx) -> Ref<GFX::GfxPipeline> {
			RHI::GraphicsPipelineDesc pipeline_descriptor{};
			if (!compile_fullscreen_stages("SkyPrefilter.slang", pipeline_descriptor)) {
				return nullptr;
			}

			pipeline_descriptor.depth_stencil.depth_test = false;
			pipeline_descriptor.depth_stencil.depth_write = false;
			pipeline_descriptor.color_formats = { RHI::TextureFormat::RGBA16F };
			pipeline_descriptor.depth_format = RHI::TextureFormat::None;
			pipeline_descriptor.set_layouts = { &SceneFrameData::get()->get_layout().get_rhi(),
												&m_cube_layout->get_rhi() };
			pipeline_descriptor.push_constants = { {
				.stages = RHI::ShaderStageFlags::Fragment,
				.offset = 0,
				.size = sizeof(PrefilterPush),
			} };
			pipeline_descriptor.debug_name = "SkyPrefilter";
			m_prefiltered = false;
			return build_ctx.create_graphics_pipeline(pipeline_descriptor);
		});
}

void SkySystem::request_image(const std::string &path) {
	if (path == m_image_path) {
		return;
	}
	m_image_path = path;
	m_image = {};
	m_image_failed = false;
	++m_image_request;
	if (path.empty()) {
		return;
	}

	auto job = [inbox = m_inbox, request = m_image_request, path,
				native_path = Platform::Filesystem::VirtualFileSystem::get()->to_native_path(path)] {
		Texture::TextureLoader::RawHDRData raw;
		try {
			raw = native_path ? Texture::TextureLoader().load_hdr_from_native(*native_path)
							  : Texture::TextureLoader().load_hdr_from_file(path);
		} catch (const std::exception &error) {
			AQUILA_LOG_WARNING("SkySystem: {}", error.what());
		}
		EnvironmentImage image;
		if (raw.is_valid()) {
			image = EnvironmentImage::build(raw.pixels.get(), raw.width, raw.height);
		} else {
			AQUILA_LOG_WARNING("SkySystem: can't decode sky image '{}'", path);
		}
		{
			std::scoped_lock lock(inbox->mutex);
			inbox->done.push_back({ .request = request, .image = std::move(image) });
		}
		Foundation::FrameScheduler::get()->request_frame();
	};
	Foundation::JobSystem::get()->schedule(Foundation::JobPriority::High, "Load sky image " + path, std::move(job));
}

void SkySystem::receive_image() {
	std::vector<ImageInbox::Finished> done;
	{
		std::scoped_lock lock(m_inbox->mutex);
		done.swap(m_inbox->done);
	}
	for (ImageInbox::Finished &finished : done) {
		if (finished.request != m_image_request) {
			continue;
		}
		if (!finished.image.is_valid()) {
			m_image_failed = true;
			continue;
		}
		RHI::SamplerDesc sampler = RHI::SamplerDesc::texture2_d(static_cast<F32>(finished.image.mips.levels.size()));
		sampler.address_v = RHI::AddressMode::ClampToEdge;
		sampler.anisotropy = false;
		m_image.texture =
			Texture::upload_mip_chain(*m_ctx, finished.image.mips, RHI::TextureFormat::RGBA16F, sampler, m_image_path);
		m_image.irradiance = finished.image.irradiance;
		m_image_failed = m_image.texture == nullptr;
	}
}

void SkySystem::bind_image(Uint32 frame_slot) {
	GFX::GfxTexture &image = m_image.texture ? *m_image.texture : *m_placeholder;
	if (m_bound_images[frame_slot] != image.get_id()) {
		m_sky_sets[frame_slot]->set_texture(1, image).flush();
		m_bound_images[frame_slot] = image.get_id();
	}
}

void SkySystem::prepare_passes(RG::RenderGraph &graph, FrameContext &ctx) {
	m_has_sky = false;
	receive_image();
	if (ctx.scene == nullptr) {
		return;
	}

	auto &registry = ctx.scene->get_registry();
	SkyLightComponent *sky = nullptr;
	entt::entity sky_entity = entt::null;
	for (auto entity : registry.view<SkyLightComponent>()) {
		auto &candidate = registry.get<SkyLightComponent>(entity);
		if (candidate.is_active() && SceneManagement::is_visible_in_hierarchy(registry, entity)) {
			sky = &candidate;
			sky_entity = entity;
			break;
		}
	}

	if (sky == nullptr) {
		return;
	}

	m_image_sky = sky->get_source() == SkySource::HdrImage;
	if (m_image_sky) {
		request_image(sky->get_hdr_path());
		if (!m_image.texture) {
			return;
		}
	}

	const bool changed = sky->is_dirty() || sky_entity != m_sky_entity;
	m_sky_entity = sky_entity;
	if (m_image_sky) {
		if (changed) {
			sky->set_irradiance(m_image.irradiance);
		}
		m_cached.tint_intensity = Vec4(sky->get_tint(), sky->get_image_intensity());
	} else {
		if (changed) {
			m_cached = HosekWilkieSky::build(Math::radians(sky->get_sun_elevation()), sky->get_turbidity(),
											 sky->get_ground_albedo(), sky->get_sun_direction());
			sky->set_irradiance(HosekWilkieSky::bake_sh(m_cached));
		}
		m_cached.tint_intensity =
			Vec4(sky->get_tint(), sky->get_intensity() * Foundation::Photometry::LUMINOUS_EFFICACY);
	}
	m_sky_buffers[ctx.frame_slot]->write(&m_cached, sizeof(GpuSkyData));
	bind_image(ctx.frame_slot);
	m_has_sky = true;

	if (changed) {
		m_prefiltered = false;
		Foundation::FrameScheduler::get()->request_frame();
	}
	if (!m_prefiltered) {
		add_bake_passes(graph, ctx.frame_slot);
	}
	SceneFrameData::get()->set_reflection_ready(ctx.frame_slot, m_prefiltered);
}

void SkySystem::add_passes(RG::RenderGraph &graph, FrameContext &ctx) {
	Shader::ReloadablePipeline *pipeline = m_image_sky ? m_image_pipeline.get() : m_pipeline.get();
	if (!m_has_sky || pipeline == nullptr || !pipeline->is_valid()) {
		return;
	}
	if (ctx.settings != nullptr &&
		(!ctx.settings->show_sky ||
		 (ctx.settings->debug_view != DebugView::Lit && ctx.settings->debug_view != DebugView::Albedo))) {
		return;
	}

	const Uint32 frame_slot = ctx.frame_slot;
	graph.add_pass(
		"Sky",
		[&ctx](RG::RGPassBuilder &builder) {
			ctx.h_scene_color = builder.set_color_attachment(0, ctx.h_scene_color, RG::AttachmentLoadOp::Load,
															 RG::AttachmentStoreOp::Store);
			ctx.h_depth =
				builder.set_depth_attachment(ctx.h_depth, RG::AttachmentLoadOp::Load, RG::AttachmentStoreOp::Store,
											 RG::AttachmentLoadOp::DontCare, RG::AttachmentStoreOp::DontCare, true);
		},
		[this, pipeline, frame_slot](GFX::GfxCommandList &cmd, RG::RGRegistry &) {
			cmd.bind_pipeline(pipeline->get());
			cmd.bind_descriptor_set(0, SceneFrameData::get()->get_descriptor_set(frame_slot));
			cmd.bind_descriptor_set(1, *m_sky_sets[frame_slot]);
			cmd.draw(3);
		});
}

void SkySystem::add_bake_passes(RG::RenderGraph &graph, Uint32 frame_slot) {
	if (!m_cube_pipeline || !m_cube_pipeline->is_valid() || !m_prefilter_pipeline ||
		!m_prefilter_pipeline->is_valid()) {
		return;
	}

	const F32 image_width = m_image_sky && m_image.texture ? static_cast<F32>(m_image.texture->get_width()) : 0.F;
	RG::RGTextureHandle source =
		graph.import_texture(m_source_cube.get(), "SkySourceCube", RG::ResourceState::ShaderRead);
	for (Uint32 mip = 0; mip < K_SOURCE_MIPS; ++mip) {
		const F32 face_size = static_cast<F32>(std::max(1U, K_SOURCE_SIZE >> mip));
		const SkyCubePush base{
			.image = m_image_sky ? 1U : 0U,
			.image_lod = image_width > 0.F ? Math::max(0.F, std::log2(image_width / (4.F * face_size))) : 0.F,
		};
		for (Uint32 face = 0; face < K_CUBE_FACES; ++face) {
			SkyCubePush push = base;
			push.face = face;
			graph.add_pass(
				"SkyCube",
				[&source, mip, face](RG::RGPassBuilder &builder) {
					source = builder.set_color_attachment(0, source, RG::AttachmentLoadOp::DontCare,
														  RG::AttachmentStoreOp::Store, {}, mip, face);
				},
				[this, frame_slot, push](GFX::GfxCommandList &cmd, RG::RGRegistry &) {
					cmd.bind_pipeline(m_cube_pipeline->get());
					cmd.bind_descriptor_set(1, *m_sky_sets[frame_slot]);
					cmd.push_constants(push, RHI::ShaderStageFlags::Fragment);
					cmd.draw(3);
				});
		}
	}

	RG::RGTextureHandle cube = graph.import_texture(&SceneFrameData::get()->get_reflection_cube(), "ReflectionCube",
													RG::ResourceState::ShaderRead);
	for (Uint32 mip = 0; mip < SceneFrameData::K_REFLECTION_MIPS; ++mip) {
		const F32 face_size = static_cast<F32>(std::max(1U, SceneFrameData::K_REFLECTION_SIZE >> mip));
		const PrefilterPush base{
			.roughness = static_cast<F32>(mip) / static_cast<F32>(SceneFrameData::K_REFLECTION_MIPS - 1),
			.source_size = static_cast<F32>(K_SOURCE_SIZE),
			.base_lod = Math::max(0.F, std::log2(static_cast<F32>(K_SOURCE_SIZE) / face_size)),
		};
		for (Uint32 face = 0; face < K_CUBE_FACES; ++face) {
			PrefilterPush push = base;
			push.face = face;
			graph.add_pass(
				"SkyPrefilter",
				[&source, &cube, mip, face](RG::RGPassBuilder &builder) {
					builder.read_texture(source);
					cube = builder.set_color_attachment(0, cube, RG::AttachmentLoadOp::DontCare,
														RG::AttachmentStoreOp::Store, {}, mip, face);
				},
				[this, push](GFX::GfxCommandList &cmd, RG::RGRegistry &) {
					cmd.bind_pipeline(m_prefilter_pipeline->get());
					cmd.bind_descriptor_set(1, *m_cube_set);
					cmd.push_constants(push, RHI::ShaderStageFlags::Fragment);
					cmd.draw(3);
				});
		}
	}

	graph.add_pass(
		"SkyPrefilterReadable",
		[&cube](RG::RGPassBuilder &builder) {
			builder.read_texture(cube);
			builder.mark_as_side_effect();
		},
		[](GFX::GfxCommandList &, RG::RGRegistry &) {});
	m_prefiltered = true;
}

} // namespace Aquila::Rendering
