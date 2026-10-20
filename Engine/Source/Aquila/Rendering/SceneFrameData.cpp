#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/Scene/Visibility.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scene/Entity.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/Components/FogComponent.h"
#include "Aquila/Scene/Components/LightComponent.h"
#include "Aquila/Scene/Components/SkyLightComponent.h"
#include "Aquila/Scene/Components/MaterialComponent.h"
#include "Aquila/Rendering/FrameData.h"
#include "Aquila/Rendering/LightData.h"
#include "Aquila/Graphics/SurfaceData.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/Photometry.h"

#include <glm/gtc/quaternion.hpp>

namespace Aquila::Rendering {

using namespace SceneManagement::Components;

SceneFrameData::SceneFrameData(GFX::GfxContext &ctx, Uint32 width, Uint32 height)
	: m_ctx(ctx), m_width(width), m_height(height) {
	m_layout = m_ctx.create_descriptor_set_layout({
        .bindings = {
            {
                .binding = 0,
                .type    = RHI::DescriptorType::UniformBuffer,
                .stages  = RHI::ShaderStageFlags::Vertex | RHI::ShaderStageFlags::Fragment | RHI::ShaderStageFlags::Compute,
                .count   = 1,
            },
            {
                .binding = 1,
                .type    = RHI::DescriptorType::StorageBuffer,
                .stages  = RHI::ShaderStageFlags::Fragment | RHI::ShaderStageFlags::Compute,
                .count   = 1,
            },
            {
                .binding = 2,
                .type    = RHI::DescriptorType::UniformBuffer,
                .stages  = RHI::ShaderStageFlags::Fragment | RHI::ShaderStageFlags::Compute,
                .count   = 1,
            },
            {
                .binding = 3,
                .type    = RHI::DescriptorType::StorageBuffer,
                .stages  = RHI::ShaderStageFlags::Vertex | RHI::ShaderStageFlags::Fragment | RHI::ShaderStageFlags::Compute,
                .count   = 1,
            },
            {
                .binding = 4,
                .type    = RHI::DescriptorType::StorageBuffer,
                .stages  = RHI::ShaderStageFlags::Fragment | RHI::ShaderStageFlags::Compute,
                .count   = 1,
            },
            {
                .binding = 5,
                .type    = RHI::DescriptorType::StorageBuffer,
                .stages  = RHI::ShaderStageFlags::Fragment | RHI::ShaderStageFlags::Compute,
                .count   = 1,
            },
            {
                .binding = 6,
                .type    = RHI::DescriptorType::UniformBuffer,
                .stages  = RHI::ShaderStageFlags::Fragment,
                .count   = 1,
            },
            {
                .binding = 7,
                .type    = RHI::DescriptorType::CombinedImageSampler,
                .stages  = RHI::ShaderStageFlags::Fragment,
                .count   = 1,
            },
            {
                .binding = 8,
                .type    = RHI::DescriptorType::CombinedImageSampler,
                .stages  = RHI::ShaderStageFlags::Fragment,
                .count   = 1,
            },
            {
                .binding = 9,
                .type    = RHI::DescriptorType::CombinedImageSampler,
                .stages  = RHI::ShaderStageFlags::Fragment,
                .count   = 1,
            },
            {
                .binding = 10,
                .type    = RHI::DescriptorType::CombinedImageSampler,
                .stages  = RHI::ShaderStageFlags::Fragment,
                .count   = 1,
            },
            {
                .binding = 11,
                .type    = RHI::DescriptorType::StorageBuffer,
                .stages  = RHI::ShaderStageFlags::Fragment,
                .count   = 1,
            },
            {
                .binding = 12,
                .type    = RHI::DescriptorType::CombinedImageSampler,
                .stages  = RHI::ShaderStageFlags::Fragment,
                .count   = 1,
            },
        },
    });

	for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
		m_frame_buffers[i] = ctx.create_buffer({
			.size = sizeof(GpuFrameData),
			.usage = RHI::BufferUsage::UniformBuffer,
			.domain = RHI::MemoryDomain::CpuToGpu,
			.debug_name = "FrameData_" + std::to_string(i),
		});

		m_light_buffers[i] = ctx.create_buffer({
			.size = sizeof(GpuLightData) * SharedConstants::MAX_LIGHTS,
			.usage = RHI::BufferUsage::StorageBuffer,
			.domain = RHI::MemoryDomain::CpuToGpu,
			.debug_name = "LightData_" + std::to_string(i),
		});

		m_env_buffers[i] = ctx.create_buffer({
			.size = sizeof(GpuEnvironmentData),
			.usage = RHI::BufferUsage::UniformBuffer,
			.domain = RHI::MemoryDomain::CpuToGpu,
			.debug_name = "EnvironmentData_" + std::to_string(i),
		});

		m_material_buffers[i] = ctx.create_buffer({
			.size = sizeof(Graphics::GpuSurfaceData) * SharedConstants::MAX_MATERIALS,
			.usage = RHI::BufferUsage::StorageBuffer,
			.domain = RHI::MemoryDomain::CpuToGpu,
			.debug_name = "MaterialData_" + std::to_string(i),
		});

		m_shadow_buffers[i] = ctx.create_buffer({
			.size = sizeof(GpuShadowData),
			.usage = RHI::BufferUsage::UniformBuffer,
			.domain = RHI::MemoryDomain::CpuToGpu,
			.debug_name = "ShadowData_" + std::to_string(i),
		});

		for (Uint32 c = 0; c < SHADOW_CASCADE_COUNT; ++c) {
			m_shadow_maps[i][c] = ctx.create_texture({
				.width = kShadowMapSize,
				.height = kShadowMapSize,
				.format = RHI::TextureFormat::Depth32,
				.usage = RHI::TextureUsage::DepthAttachment | RHI::TextureUsage::Sampled,
				.view_type = RHI::TextureViewType::Tex2D,
				.sampler = RHI::SamplerDesc::point_sample(),
				.debug_name = "ShadowMap_" + std::to_string(i) + "_" + std::to_string(c),
			});
		}

		m_local_shadow_buffers[i] = ctx.create_buffer({
			.size = sizeof(GpuLocalShadow) * (kLocalShadowAtlasSize / kLocalShadowTileSize) *
				(kLocalShadowAtlasSize / kLocalShadowTileSize),
			.usage = RHI::BufferUsage::StorageBuffer,
			.domain = RHI::MemoryDomain::CpuToGpu,
			.debug_name = "LocalShadows_" + std::to_string(i),
		});

		m_local_shadow_atlases[i] = ctx.create_texture({
			.width = kLocalShadowAtlasSize,
			.height = kLocalShadowAtlasSize,
			.format = RHI::TextureFormat::Depth32,
			.usage = RHI::TextureUsage::DepthAttachment | RHI::TextureUsage::Sampled,
			.view_type = RHI::TextureViewType::Tex2D,
			.sampler = RHI::SamplerDesc::point_sample(),
			.debug_name = "LocalShadowAtlas_" + std::to_string(i),
		});

		m_sets[i] = ctx.allocate_descriptor_set(*m_layout);
	}

	m_light_index_list_buffer = ctx.create_buffer({
		.size = sizeof(Uint32) * SharedConstants::CLUSTER_COUNT * SharedConstants::MAX_LIGHTS_PER_CLUSTER,
		.usage = RHI::BufferUsage::StorageBuffer,
		.domain = RHI::MemoryDomain::GpuOnly,
		.debug_name = "LightIndexList",
	});
	m_cluster_light_info_buffer = ctx.create_buffer({
		.size = 2 * sizeof(Uint32) * SharedConstants::CLUSTER_COUNT,
		.usage = RHI::BufferUsage::StorageBuffer,
		.domain = RHI::MemoryDomain::GpuOnly,
		.debug_name = "ClusterLightInfo",
	});

	for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
		m_sets[i]
			->set_buffer(0, *m_frame_buffers[i])
			.set_buffer(1, *m_light_buffers[i])
			.set_buffer(2, *m_env_buffers[i])
			.set_buffer(3, *m_material_buffers[i])
			.set_buffer(4, *m_light_index_list_buffer)
			.set_buffer(5, *m_cluster_light_info_buffer)
			.set_buffer(6, *m_shadow_buffers[i])
			.set_texture(7, *m_shadow_maps[i][0])
			.set_texture(8, *m_shadow_maps[i][1])
			.set_texture(9, *m_shadow_maps[i][2])
			.set_texture(10, *m_shadow_maps[i][3])
			.set_buffer(11, *m_local_shadow_buffers[i])
			.set_texture(12, *m_local_shadow_atlases[i])
			.flush();
	}
}

RenderView render_view_from_entity(const CameraComponent &cam, const TransformComponent &transform) {
	const Vec3 pos = transform.get_world_position();
	const quat rot = transform.get_local_rotation();

	RenderView view;
	view.view = cam.get_view_matrix(pos, rot);
	view.projection = cam.get_projection_matrix();
	view.position = pos;
	view.forward = cam.get_forward_direction(rot);
	view.up = cam.get_up_direction(rot);
	view.right = cam.get_right_direction(rot);
	view.near_plane = cam.near_plane;
	view.far_plane = cam.far_plane;
	view.fov = cam.fov;
	view.aspect = cam.aspect_ratio;
	view.ev100 = cam.ev100;
	view.exposure_compensation = cam.exposure_compensation;
	view.auto_exposure = cam.auto_exposure;
	view.is_orthographic = cam.is_orthographic;
	view.valid = true;
	return view;
}

static GpuCameraData build_camera_data(const RenderView &view, Uint32 index, Uint32 width, Uint32 height) {
	GpuCameraData data{};

	data.view = view.view;
	data.projection = view.projection;
	data.view_projection = data.projection * data.view;
	data.inverse_view = Math::inverse(data.view);
	data.inverse_projection = Math::inverse(data.projection);
	data.inverse_view_projection = Math::inverse(data.view_projection);
	data.prev_view_projection = data.view_projection;

	data.position = Vec4(view.position, 0.F);
	data.forward = Vec4(view.forward, 0.F);
	data.up = Vec4(view.up, 0.F);
	data.right = Vec4(view.right, 0.F);
	data.near_plane = view.near_plane;
	data.far_plane = view.far_plane;
	data.fov = view.fov;
	data.aspect_ratio = view.aspect;
	data.resolution = Vec2(static_cast<float>(width), static_cast<float>(height));
	data.jitter = Vec2(0.F);
	data.is_orthographic = view.is_orthographic ? 1u : 0u;
	data.camera_index = index;

	return data;
}

static Vec3 light_direction(const LightComponent &light) {
	const F32 length = Math::length(light.m_direction);
	return length > 1e-6F ? light.m_direction / length : Vec3(0.F, -1.F, 0.F);
}

static F32 shader_intensity(const LightComponent &light) {
	switch (light.m_type) {
	case LightComponent::Type::Point:
		return Foundation::Photometry::point_intensity(light.m_intensity);
	case LightComponent::Type::Spot:
		return Foundation::Photometry::spot_intensity(light.m_intensity, Math::radians(light.m_outer_cone_angle));
	default:
		return light.m_intensity;
	}
}

static GpuLightData build_light_data(const LightComponent &light, const TransformComponent &transform) {
	GpuLightData data{};

	data.m_position_and_range = Vec4(transform.get_world_position(), light.m_range);
	data.m_color_and_intensity = Vec4(light.get_emitted_color(), shader_intensity(light));
	data.m_direction_and_type = Vec4(light_direction(light), static_cast<float>(light.m_type));
	data.m_shadow_index = -1;
	data.m_flags = 0;

	switch (light.m_type) {
	case LightComponent::Type::Spot: {
		data.m_cos_inner_angle = Math::cos(Math::radians(light.m_inner_cone_angle));
		data.m_cos_outer_angle = Math::cos(Math::radians(light.m_outer_cone_angle));
		break;
	}
	case LightComponent::Type::Area: {
		const Mat3 rot = glm::mat3_cast(transform.get_world_rotation());
		const Vec3 right = rot[0];
		const Vec3 up = rot[1];
		data.m_right_and_width = Vec4(right, light.m_area_size.x);
		data.m_up_and_height = Vec4(up, light.m_area_size.y);
		data.m_cos_inner_angle = 1.0F;
		data.m_cos_outer_angle = 1.0F;
		break;
	}
	default:
		data.m_cos_inner_angle = 1.0F;
		data.m_cos_outer_angle = 1.0F;
		break;
	}

	if (light.m_type == LightComponent::Type::Directional) {
		const F32 half_angle = Math::radians(Math::max(light.m_angular_diameter, 0.F) * 0.5F);
		data.m_source = Vec4(Math::sin(half_angle), Math::cos(half_angle), 0.F, 0.F);
	} else {
		data.m_source = Vec4(Math::max(light.m_source_radius, 0.F), 0.F, 0.F, 0.F);
	}

	return data;
}

static Graphics::GpuSurfaceData surface_of(const MaterialComponent &component) {
	Graphics::GpuSurfaceData surface;
	if (!component.material) {
		return surface;
	}
	const Graphics::Material &material = *component.material;
	auto vec4_of = [&material](const char *name) -> Option<Vec4> {
		const Graphics::MaterialParameter *param = material.get_parameter(name);
		if (param == nullptr) {
			return std::nullopt;
		}
		const Graphics::MaterialValue value = material.get_value(*param);
		if (const auto *v = std::get_if<Vec4>(&value)) {
			return *v;
		}
		if (const auto *v = std::get_if<Vec3>(&value)) {
			return Vec4(*v, 1.F);
		}
		return std::nullopt;
	};
	auto float_of = [&material](const char *name) -> Option<F32> {
		const Graphics::MaterialParameter *param = material.get_parameter(name);
		if (param == nullptr) {
			return std::nullopt;
		}
		const Graphics::MaterialValue value = material.get_value(*param);
		if (const auto *f = std::get_if<F32>(&value)) {
			return *f;
		}
		return std::nullopt;
	};
	if (Option<Vec4> color = vec4_of("baseColor")) {
		surface.albedo = *color;
	} else if (Option<Vec4> unlit = vec4_of("color")) {
		surface.albedo = *unlit;
	}
	surface.metallic = float_of("metallic").value_or(surface.metallic);
	surface.roughness = float_of("roughness").value_or(surface.roughness);
	return surface;
}

struct LocalCaster {
	Uint32 light_index = 0;
	const LightComponent *light = nullptr;
	Vec3 position = Vec3(0.F);
	F32 priority = 0.F;
};

static Vec3 face_up(const Vec3 &direction) {
	return Math::abs(direction.y) > 0.99F ? Vec3(0.F, 0.F, 1.F) : Vec3(0.F, 1.F, 0.F);
}

static GpuShadowData compute_shadow_data(const RenderView &camera, const Vec3 &light_dir, F32 angular_diameter,
										 Uint32 shadow_map_size, std::array<Mat4, SHADOW_CASCADE_COUNT> &out_vp) {
	constexpr F32 kSplitLambda = 0.95F;
	constexpr F32 kDepthBias = 0.0015F;
	constexpr F32 kNormalBias = 0.03F;
	constexpr F32 kMaxShadowDistance = 100.0F;

	const F32 light_tan = std::tan(Math::radians(Math::clamp(angular_diameter, 0.F, 20.F) * 0.5F));

	GpuShadowData sd{};
	sd.m_num_cascades = static_cast<Int32>(SHADOW_CASCADE_COUNT);
	sd.m_light_direction = Vec4(Math::normalize(light_dir), 0.F);
	sd.m_params = Vec4(kDepthBias, kNormalBias, light_tan, static_cast<F32>(shadow_map_size));

	const F32 near_plane = camera.near_plane;
	const F32 far_plane = Math::min(camera.far_plane, kMaxShadowDistance);

	F32 splits[SHADOW_CASCADE_COUNT];
	for (Uint32 i = 0; i < SHADOW_CASCADE_COUNT; ++i) {
		const F32 p = static_cast<F32>(i + 1) / static_cast<F32>(SHADOW_CASCADE_COUNT);
		const F32 log_split = near_plane * Math::pow(far_plane / near_plane, p);
		const F32 uni_split = near_plane + (far_plane - near_plane) * p;
		splits[i] = kSplitLambda * log_split + (1.0F - kSplitLambda) * uni_split;
	}

	const Mat4 cam_view = camera.view;
	const Vec3 ld = Math::normalize(light_dir);
	Vec3 up = (Math::abs(ld.y) > 0.99F) ? Vec3(0.F, 0.F, 1.F) : Vec3(0.F, 1.F, 0.F);

	F32 last_split = near_plane;
	for (Uint32 i = 0; i < SHADOW_CASCADE_COUNT; ++i) {
		const F32 split_near = last_split;
		const F32 split_far = splits[i];

		const Mat4 sub_proj =
			Math::perspective_vulkan(Math::radians(camera.fov), camera.aspect, split_near, split_far);
		const std::array<Vec4, 8> corners = Math::extract_frustum_corners(sub_proj * cam_view);

		Vec3 center(0.F);
		for (const auto &corner : corners) {
			center += Vec3(corner);
		}
		center /= 8.0F;

		F32 radius = 0.F;
		for (const auto &corner : corners) {
			radius = Math::max(radius, Math::length(Vec3(corner) - center));
		}
		radius = Math::ceil(radius * 16.0F) / 16.0F;

		const F32 back_dist = radius * 3.0F;
		const Vec3 eye = center - ld * back_dist;
		const Mat4 light_view = Math::look_in_direction(eye, ld, up);
		Mat4 light_proj = Math::ortho_vulkan(-radius, radius, -radius, radius, 0.F, back_dist + radius);

		Mat4 view_proj = light_proj * light_view;
		Vec4 origin = view_proj * Vec4(0.F, 0.F, 0.F, 1.F);
		origin *= static_cast<F32>(shadow_map_size) / 2.0F;
		const Vec2 rounded = Math::round(Vec2(origin.x, origin.y));
		const Vec2 offset = (rounded - Vec2(origin.x, origin.y)) * (2.0F / static_cast<F32>(shadow_map_size));
		light_proj[3][0] += offset.x;
		light_proj[3][1] += offset.y;

		view_proj = light_proj * light_view;
		out_vp[i] = view_proj;
		sd.m_cascade_view_proj[i] = view_proj;
		sd.m_cascade_splits[i] = split_far;
		last_split = split_far;
	}

	return sd;
}

void SceneFrameData::update(SceneManagement::Scene &scene, float delta_time, Uint32 frame_slot,
							const RenderView &primary_view, F32 exposure, bool shadows_allowed) {
	m_time += delta_time;

	GpuFrameData gpu_frame{};
	gpu_frame.time = m_time;
	gpu_frame.delta_time = delta_time;
	gpu_frame.frame_index = m_frame_index++;
	gpu_frame.screen_resolution = Vec2(static_cast<float>(m_width), static_cast<float>(m_height));
	gpu_frame.exposure = exposure;

	gpu_frame.main_camera = build_camera_data(primary_view, 0, m_width, m_height);

	auto &registry = scene.get_registry();
	{
		auto view = registry.view<CameraComponent, TransformComponent>();
		Uint32 count = 0;
		for (auto entity : view) {
			if (count >= SharedConstants::MAX_CAMERAS) {
				break;
			}
			const RenderView scene_view =
				render_view_from_entity(view.get<CameraComponent>(entity), view.get<TransformComponent>(entity));
			gpu_frame.cameras[count] = build_camera_data(scene_view, count, m_width, m_height);
			++count;
		}
		gpu_frame.camera_count = count;
	}

	GpuLightData lights[SharedConstants::MAX_LIGHTS];
	Uint32 light_count = 0;
	int caster_index = -1;
	Vec3 caster_dir(0.F, -1.F, 0.F);
	const LightComponent *caster = nullptr;
	std::vector<LocalCaster> local_casters;
	{
		auto view = registry.view<LightComponent, TransformComponent>();
		for (auto entity : view) {
			if (light_count >= SharedConstants::MAX_LIGHTS) {
				break;
			}
			if (!SceneManagement::is_visible_in_hierarchy(registry, entity)) {
				continue;
			}
			const auto &light = view.get<LightComponent>(entity);
			const auto &transform = view.get<TransformComponent>(entity);
			if (!light.m_is_active) {
				continue;
			}
			const Uint32 index = light_count;
			lights[light_count++] = build_light_data(light, transform);
			const bool local = light.m_type == LightComponent::Type::Point || light.m_type == LightComponent::Type::Spot;
			if (local && light.m_cast_shadows && light.m_range > 0.F) {
				const Vec3 position = transform.get_world_position();
				local_casters.push_back({
					.light_index = index,
					.light = &light,
					.position = position,
					.priority = Math::length(position - primary_view.position) - light.m_range,
				});
			}
			if (caster_index < 0 && light.m_type == LightComponent::Type::Directional) {
				caster_index = static_cast<int>(index);
				caster_dir = light_direction(light);
				caster = &light;
			}
		}
	}
	gpu_frame.light_count = light_count;

	GpuShadowData shadow_data{};
	m_shadows_enabled = false;
	if (shadows_allowed && primary_view.valid && caster_index >= 0 && caster->m_cast_shadows) {
		shadow_data = compute_shadow_data(primary_view, caster_dir, caster->m_angular_diameter, kShadowMapSize,
										  m_cascade_view_proj);
		shadow_data.m_enabled = 1;
		lights[caster_index].m_shadow_index = 0;
		m_shadows_enabled = true;
	}
	m_shadow_buffers[frame_slot]->write(&shadow_data, sizeof(GpuShadowData));

	m_local_shadow_views.clear();
	std::vector<GpuLocalShadow> local_shadows;
	if (shadows_allowed && primary_view.valid && !local_casters.empty()) {
		constexpr Uint32 tiles_per_row = kLocalShadowAtlasSize / kLocalShadowTileSize;
		constexpr Uint32 tile_count = tiles_per_row * tiles_per_row;
		constexpr F32 atlas_size = static_cast<F32>(kLocalShadowAtlasSize);
		constexpr F32 tile_size = static_cast<F32>(kLocalShadowTileSize);
		static const std::array<Vec3, 6> k_face_directions = { {
			{ 1.F, 0.F, 0.F },
			{ -1.F, 0.F, 0.F },
			{ 0.F, 1.F, 0.F },
			{ 0.F, -1.F, 0.F },
			{ 0.F, 0.F, 1.F },
			{ 0.F, 0.F, -1.F },
		} };
		const F32 point_fov = 2.F * std::atan(tile_size / (tile_size - 2.F * static_cast<F32>(kLocalShadowGuardTexels)));

		std::ranges::sort(local_casters, {}, &LocalCaster::priority);
		Uint32 next_tile = 0;
		for (const LocalCaster &local : local_casters) {
			const LightComponent &light = *local.light;
			const bool point = light.m_type == LightComponent::Type::Point;
			const Uint32 needed = point ? 6U : 1U;
			if (next_tile + needed > tile_count) {
				continue;
			}

			const F32 near_plane = Math::clamp(light.m_source_radius, 0.05F, Math::max(light.m_range * 0.25F, 0.05F));
			const F32 far_plane = Math::max(light.m_range, near_plane * 2.F);
			const F32 spot_half_angle = Math::min(Math::max(light.m_outer_cone_angle, 1.F) + 3.F, 80.F);
			const F32 fov = point ? point_fov : 2.F * Math::radians(spot_half_angle);
			const Mat4 projection = Math::perspective_vulkan(fov, 1.F, near_plane, far_plane);

			lights[local.light_index].m_shadow_index = static_cast<Int32>(next_tile);
			for (Uint32 face = 0; face < needed; ++face) {
				const Uint32 tile = next_tile + face;
				const Vec3 direction = point ? k_face_directions[face] : light_direction(light);
				const Mat4 view_proj =
					projection * Math::look_in_direction(local.position, direction, face_up(direction));
				const Uint32 x = (tile % tiles_per_row) * kLocalShadowTileSize;
				const Uint32 y = (tile / tiles_per_row) * kLocalShadowTileSize;

				GpuLocalShadow gpu{};
				for (int column = 0; column < 4; ++column) {
					gpu.m_view_proj[column] = view_proj[column];
				}
				gpu.m_rect = Vec4(static_cast<F32>(x) / atlas_size, static_cast<F32>(y) / atlas_size,
								  tile_size / atlas_size, tile_size / atlas_size);
				gpu.m_params = Vec4(near_plane, far_plane, Math::max(light.m_source_radius, 0.F),
									1.F / std::tan(fov * 0.5F));
				gpu.m_extra = Vec4(tile_size, atlas_size, 0.F, 0.F);
				local_shadows.push_back(gpu);

				m_local_shadow_views.push_back({
					.view_proj = view_proj,
					.x = x,
					.y = y,
					.size = kLocalShadowTileSize,
					.light_position = local.position,
					.range = far_plane,
				});
			}
			next_tile += needed;
		}
	}
	if (!local_shadows.empty()) {
		m_local_shadow_buffers[frame_slot]->write(local_shadows.data(), sizeof(GpuLocalShadow) * local_shadows.size());
	}

	m_frame_buffers[frame_slot]->write(&gpu_frame, sizeof(GpuFrameData));
	if (light_count > 0) {
		m_light_buffers[frame_slot]->write(lights, sizeof(GpuLightData) * light_count);
	}

	GpuEnvironmentData env_data{};
	{
		auto view = registry.view<SkyLightComponent>();
		for (auto entity : view) {
			if (!SceneManagement::is_visible_in_hierarchy(registry, entity)) {
				continue;
			}
			const auto &sky = view.get<SkyLightComponent>(entity);
			if (!sky.is_active()) {
				continue;
			}
			const auto &sh = sky.get_irradiance();
			for (int i = 0; i < 9; ++i) {
				env_data.m_sh_coeffs[i] = Vec4(sh.coeffs[i], 0.F);
			}
			env_data.m_tint_and_intensity = Vec4(sky.get_tint(), sky.get_intensity());
			env_data.m_enabled = 1;
			break;
		}
	}
	{
		auto view = registry.view<FogComponent>();
		for (auto entity : view) {
			const auto &fog = view.get<FogComponent>(entity);
			if (!fog.active || !SceneManagement::is_visible_in_hierarchy(registry, entity)) {
				continue;
			}
			env_data.m_fog_color_density = Vec4(fog.color, Math::max(fog.density, 0.F));
			env_data.m_fog_params = Vec4(Math::max(fog.height_falloff, 0.F), fog.base_height,
										 Math::max(fog.start_distance, 0.F), Math::clamp(fog.max_opacity, 0.F, 1.F));
			env_data.m_fog_sun_direction = Vec4(-caster_dir, Math::clamp(fog.anisotropy, -0.95F, 0.95F));
			const Vec3 sun = caster != nullptr ? caster->get_emitted_color() * caster->m_intensity : Vec3(0.F);
			env_data.m_fog_sun_illuminance = Vec4(sun, 1.F);
			break;
		}
	}
	m_env_buffers[frame_slot]->write(&env_data, sizeof(GpuEnvironmentData));

	{
		Graphics::GpuSurfaceData materials[SharedConstants::MAX_MATERIALS];
		Uint32 material_count = 0;
		auto mat_view = registry.view<MaterialComponent>();
		for (auto entity : mat_view) {
			if (material_count >= SharedConstants::MAX_MATERIALS) {
				break;
			}
			auto &comp = mat_view.get<MaterialComponent>(entity);
			materials[material_count] = surface_of(comp);
			comp.material_index = material_count++;
		}
		if (material_count > 0) {
			m_material_buffers[frame_slot]->write(materials, sizeof(Graphics::GpuSurfaceData) * material_count);
		}
	}
}

void SceneFrameData::on_resize(Uint32 width, Uint32 height) {
	m_width = width;
	m_height = height;
}

GFX::GfxDescriptorSet &SceneFrameData::get_descriptor_set(Uint32 frame_slot) const {
	return *m_sets[frame_slot];
}

} // namespace Aquila::Rendering
