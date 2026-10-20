#include "Aquila/Rendering/MaterialPreview.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/Photometry.h"
#include "Aquila/GFX/GfxBuffer.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxDescriptorSet.h"
#include "Aquila/GFX/GfxMesh.h"
#include "Aquila/GFX/GfxPipeline.h"
#include "Aquila/GFX/GfxRenderpass.h"
#include "Aquila/Graphics/Material/Material.h"
#include "Aquila/Graphics/Resources/MeshLibrary.h"
#include "Aquila/Graphics/SurfaceData.h"
#include "Aquila/Graphics/Texture/TextureLibrary.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Rendering/FrameData.h"
#include "Aquila/Rendering/LightData.h"
#include "Aquila/Rendering/MeshPushConstants.h"
#include "Aquila/Rendering/RenderSettings.h"
#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/Scene/Components/LightComponent.h"

namespace Aquila::Rendering {

namespace {

constexpr Uint32 k_frame_slot = 0;
constexpr F32 k_camera_distance = 4.4F;
constexpr F32 k_fov_degrees = 30.F;
constexpr F32 k_near_plane = 0.1F;
constexpr F32 k_far_plane = 20.F;
constexpr F32 k_ev100 = 12.F;
constexpr F32 k_key_lux = 10000.F;
constexpr F32 k_key_angular_diameter = 3.F;
const Vec3 k_sky_nits{ 600.F, 620.F, 680.F };
const Vec3 k_sky_gradient_nits{ 320.F, 330.F, 360.F };
const Vec4 k_backdrop{ 0.03F, 0.03F, 0.035F, 1.F };
constexpr F32 k_sh_band0 = 0.282095F;
constexpr F32 k_sh_band1 = 0.488603F;

struct ToneMapPushConstants {
	Uint32 tone_mapper = 0;
	Uint32 display_raw = 0;
};

GpuCameraData preview_camera(Uint32 size) {
	Mat4 world(1.F);
	world[3] = Vec4(0.F, 0.F, -k_camera_distance, 1.F);

	GpuCameraData data{};
	data.view = Math::inverse(world);
	data.projection =
		Math::perspective_vulkan(Math::radians(k_fov_degrees), 1.F, k_near_plane, k_far_plane);
	data.projection[1][1] *= -1.F;
	data.view_projection = data.projection * data.view;
	data.inverse_view = world;
	data.inverse_projection = Math::inverse(data.projection);
	data.inverse_view_projection = Math::inverse(data.view_projection);
	data.prev_view_projection = data.view_projection;
	data.position = world[3];
	data.forward = Vec4(0.F, 0.F, 1.F, 0.F);
	data.up = Vec4(0.F, 1.F, 0.F, 0.F);
	data.right = Vec4(1.F, 0.F, 0.F, 0.F);
	data.near_plane = k_near_plane;
	data.far_plane = k_far_plane;
	data.fov = k_fov_degrees;
	data.aspect_ratio = 1.F;
	data.resolution = Vec2(static_cast<F32>(size));
	return data;
}

GpuLightData key_light() {
	const F32 half_angle = Math::radians(k_key_angular_diameter * 0.5F);
	GpuLightData light{};
	light.m_color_and_intensity = Vec4(1.F, 0.97F, 0.93F, k_key_lux);
	light.m_direction_and_type = Vec4(Math::normalize(Vec3(0.55F, -0.6F, 0.6F)),
									  static_cast<F32>(SceneManagement::Components::LightComponent::Type::Directional));
	light.m_cos_inner_angle = 1.F;
	light.m_cos_outer_angle = 1.F;
	light.m_shadow_index = -1;
	light.m_source = Vec4(Math::sin(half_angle), Math::cos(half_angle), 0.F, 0.F);
	return light;
}

GpuEnvironmentData studio_environment() {
	GpuEnvironmentData environment{};
	environment.m_sh_coeffs[0] = Vec4(k_sky_nits / k_sh_band0, 0.F);
	environment.m_sh_coeffs[1] = Vec4(-k_sky_gradient_nits / k_sh_band1, 0.F);
	environment.m_tint_and_intensity = Vec4(1.F);
	environment.m_enabled = 1;
	return environment;
}

}

MaterialPreview::MaterialPreview(GFX::GfxContext &ctx, Uint32 size) : m_ctx(ctx), m_size(std::max(size, 8U)) {}

MaterialPreview::~MaterialPreview() = default;

Ref<GFX::GfxBuffer> MaterialPreview::make_buffer(Uint64 size, bool uniform, const void *data, const char *name) {
	Ref<GFX::GfxBuffer> buffer = m_ctx.create_buffer({
		.size = size,
		.usage = uniform ? RHI::BufferUsage::UniformBuffer : RHI::BufferUsage::StorageBuffer,
		.domain = RHI::MemoryDomain::CpuToGpu,
		.debug_name = name,
	});
	if (buffer && data != nullptr) {
		buffer->write(data, size);
	}
	m_buffers.push_back(buffer);
	return buffer;
}

void MaterialPreview::create_resources() {
	m_created = true;
	const Graphics::Resources::MeshParams params{ { "radius", 1.F }, { "segments", 64.F }, { "rings", 32.F } };
	Ref<Graphics::Resources::Mesh> mesh =
		Graphics::Resources::MeshLibrary::instance().load(Graphics::Resources::MeshSource::procedural("sphere", params));
	if (!mesh) {
		return;
	}
	m_sphere = GFX::GfxMesh::create(m_ctx, mesh->get_vertices(), mesh->get_indices());

	RHI::TextureDesc color_desc{};
	color_desc.width = m_size;
	color_desc.height = m_size;
	color_desc.format = RHI::TextureFormat::RGBA16F;
	color_desc.usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;
	color_desc.sampler = RHI::SamplerDesc::render_target();
	color_desc.debug_name = "MaterialPreviewColor";
	m_color = m_ctx.create_texture(color_desc);

	RHI::TextureDesc depth_desc{};
	depth_desc.width = m_size;
	depth_desc.height = m_size;
	depth_desc.format = RHI::TextureFormat::Depth32;
	depth_desc.usage = RHI::TextureUsage::DepthAttachment;
	depth_desc.debug_name = "MaterialPreviewDepth";
	m_depth = m_ctx.create_texture(depth_desc);
	if (!m_color || !m_depth) {
		return;
	}

	const auto scene_pass = [this](Vec4 clear) {
		RHI::RenderPassDesc desc{};
		desc.color_attachments = { {
			.texture = &m_color->get_rhi(),
			.clear_color = clear,
			.load_op = RHI::AttachmentLoadOp::Clear,
			.store_op = RHI::AttachmentStoreOp::Store,
		} };
		desc.depth_attachment = RHI::RenderPassDepthAttachmentDesc{
			.texture = &m_depth->get_rhi(),
			.depth_load_op = RHI::AttachmentLoadOp::Clear,
			.depth_store_op = RHI::AttachmentStoreOp::DontCare,
		};
		desc.width = m_size;
		desc.height = m_size;
		desc.debug_name = "MaterialPreview";
		return m_ctx.create_render_pass(desc);
	};
	m_scene_pass = scene_pass(Vec4(0.F));
	m_backdrop_pass = scene_pass(k_backdrop);

	GpuFrameData frame{};
	frame.main_camera = preview_camera(m_size);
	frame.screen_resolution = Vec2(static_cast<F32>(m_size));
	frame.light_count = 1;
	frame.exposure = Foundation::Photometry::exposure(k_ev100);

	const GpuLightData light = key_light();
	const GpuEnvironmentData environment = studio_environment();
	const Graphics::GpuSurfaceData surface{};
	const Uint32 light_index = 0;
	const std::vector<Uint32> clusters = [] {
		std::vector<Uint32> out(static_cast<Usize>(SharedConstants::CLUSTER_COUNT) * 2);
		for (Usize i = 0; i < SharedConstants::CLUSTER_COUNT; ++i) {
			out[i * 2] = 0;
			out[i * 2 + 1] = 1;
		}
		return out;
	}();
	const GpuShadowData shadows{};
	const GpuLocalShadow local_shadow{};

	auto frame_buffer = make_buffer(sizeof(GpuFrameData), true, &frame, "MaterialPreviewFrame");
	auto light_buffer = make_buffer(sizeof(GpuLightData), false, &light, "MaterialPreviewLights");
	auto environment_buffer = make_buffer(sizeof(GpuEnvironmentData), true, &environment, "MaterialPreviewEnvironment");
	auto surface_buffer = make_buffer(sizeof(Graphics::GpuSurfaceData), false, &surface, "MaterialPreviewSurfaces");
	auto index_buffer = make_buffer(sizeof(Uint32), false, &light_index, "MaterialPreviewLightIndices");
	auto cluster_buffer =
		make_buffer(clusters.size() * sizeof(Uint32), false, clusters.data(), "MaterialPreviewClusters");
	auto shadow_buffer = make_buffer(sizeof(GpuShadowData), true, &shadows, "MaterialPreviewShadows");
	auto local_shadow_buffer = make_buffer(sizeof(GpuLocalShadow), false, &local_shadow, "MaterialPreviewLocalShadows");
	if (std::ranges::any_of(m_buffers, [](const Ref<GFX::GfxBuffer> &buffer) { return !buffer; })) {
		return;
	}

	GFX::GfxTexture &blank = Graphics::TextureLibrary::get()->get_default(m_ctx, Graphics::DefaultTexture::White);
	m_scene_set = m_ctx.allocate_descriptor_set(SceneFrameData::get()->get_layout());
	m_scene_set->set_buffer(0, *frame_buffer)
		.set_buffer(1, *light_buffer)
		.set_buffer(2, *environment_buffer)
		.set_buffer(3, *surface_buffer)
		.set_buffer(4, *index_buffer)
		.set_buffer(5, *cluster_buffer)
		.set_buffer(6, *shadow_buffer)
		.set_texture(7, blank)
		.set_texture(8, blank)
		.set_texture(9, blank)
		.set_texture(10, blank)
		.set_buffer(11, *local_shadow_buffer)
		.set_texture(12, blank)
		.flush();

	m_tone_map_layout = m_ctx.create_descriptor_set_layout({
		.bindings = { {
			.binding = 0,
			.type = RHI::DescriptorType::CombinedImageSampler,
			.stages = RHI::ShaderStageFlags::Fragment,
			.count = 1,
		} },
	});
	m_tone_map_set = m_ctx.allocate_descriptor_set(*m_tone_map_layout);
	m_tone_map_set->set_texture(0, *m_color).flush();

	std::vector<RHI::VulkanCompiledStage> stages;
	std::string error;
	if (!RHI::VulkanShaderCompiler::compile_file(Foundation::EnginePaths::shaders_dir() + "ToneMap.slang", stages,
												 error)) {
		AQUILA_LOG_ERROR("MaterialPreview: tone map shader failed: {}", error);
		return;
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
	desc.set_layouts = { &m_tone_map_layout->get_rhi() };
	desc.push_constants = { { .stages = RHI::ShaderStageFlags::Fragment,
							  .offset = 0,
							  .size = sizeof(ToneMapPushConstants) } };
	desc.debug_name = "MaterialPreviewToneMap";
	m_tone_map_pipeline = m_ctx.create_graphics_pipeline(desc);
}

Ref<GFX::GfxTexture> MaterialPreview::render(Graphics::Material &material, Ref<GFX::GfxTexture> target) {
	if (material.get_domain() == Graphics::Shader::ShaderDomain::Fullscreen) {
		return nullptr;
	}
	if (!m_created) {
		create_resources();
	}
	if (!m_sphere || !m_scene_set || !m_tone_map_pipeline) {
		return nullptr;
	}

	Ref<GFX::GfxTexture> output = std::move(target);
	if (!output || output->get_width() != m_size || output->get_height() != m_size) {
		RHI::TextureDesc output_desc{};
		output_desc.width = m_size;
		output_desc.height = m_size;
		output_desc.format = DISPLAY_COLOR_FORMAT;
		output_desc.usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;
		output_desc.sampler = RHI::SamplerDesc::render_target();
		output_desc.debug_name = "MaterialThumbnail";
		output = m_ctx.create_texture(output_desc);
	}
	if (!output) {
		return nullptr;
	}

	RHI::RenderPassDesc tone_map_desc{};
	tone_map_desc.color_attachments = { {
		.texture = &output->get_rhi(),
		.clear_color = Vec4(0.F),
		.load_op = RHI::AttachmentLoadOp::Clear,
		.store_op = RHI::AttachmentStoreOp::Store,
	} };
	tone_map_desc.width = m_size;
	tone_map_desc.height = m_size;
	tone_map_desc.debug_name = "MaterialPreviewToneMap";
	Ref<GFX::GfxRenderPass> tone_map_pass = m_ctx.create_render_pass(tone_map_desc);

	const Graphics::BlendMode blend = material.get_blend_mode();
	const bool transparent = blend == Graphics::BlendMode::Transparent;
	GFX::GfxRenderPass &scene_pass = transparent ? *m_backdrop_pass : *m_scene_pass;
	const MeshPushConstants push{
		.model = Mat4(1.F),
		.material_index = 0,
		.receives_shadows = 0,
		.alpha_cutoff = blend == Graphics::BlendMode::Masked ? material.get_data().alpha_cutoff : -1.F,
		.transparent = transparent ? 1U : 0U,
	};
	const ToneMapPushConstants tone_map{ .tone_mapper = static_cast<Uint32>(ToneMapper::AgX), .display_raw = 0 };
	const F32 size = static_cast<F32>(m_size);

	m_ctx.wait_idle();
	material.flush(k_frame_slot);
	m_ctx.execute_immediate(RHI::CommandListType::Graphics, [&](GFX::GfxCommandList &cmd) {
		scene_pass.begin(cmd);
		cmd.set_viewport(0.F, 0.F, size, size);
		cmd.set_scissor(0, 0, m_size, m_size);
		material.bind(cmd, 1, k_frame_slot);
		cmd.bind_descriptor_set(0, *m_scene_set);
		cmd.push_constants(push, RHI::ShaderStageFlags::Vertex | RHI::ShaderStageFlags::Fragment);
		cmd.bind_vertex_buffer(m_sphere->get_vertex_buffer());
		cmd.bind_index_buffer(m_sphere->get_index_buffer());
		cmd.draw_indexed(m_sphere->get_index_count());
		scene_pass.end(cmd);

		tone_map_pass->begin(cmd);
		cmd.set_viewport(0.F, 0.F, size, size);
		cmd.set_scissor(0, 0, m_size, m_size);
		cmd.bind_pipeline(*m_tone_map_pipeline);
		cmd.bind_descriptor_set(0, *m_tone_map_set);
		cmd.push_constants(tone_map, RHI::ShaderStageFlags::Fragment);
		cmd.draw(3);
		tone_map_pass->end(cmd);
	});
	return output;
}

}
