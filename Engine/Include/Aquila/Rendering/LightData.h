#pragma once
#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Rendering {

enum class GpuLightType : Int8 {
	Point = 0,
	Directional = 1,
	Spot = 2,
	Area = 3,
};

struct alignas(16) GpuLightData {
	Vec4 m_position_and_range;
	Vec4 m_color_and_intensity;
	Vec4 m_direction_and_type;
	Vec4 m_right_and_width;
	Vec4 m_up_and_height;
	F32 m_cos_inner_angle;
	F32 m_cos_outer_angle;
	Int32 m_shadow_index;
	Uint32 m_flags;
	/// Point and Spot: x = sphere radius in meters. Directional: x = sin, y = cos of the sun's angular radius.
	Vec4 m_source;
};

static_assert(sizeof(GpuLightData) == 112, "GpuLightData must be 112 bytes for std430 packing");

struct alignas(16) GpuEnvironmentData {
	std::array<Vec4, 9> m_sh_coeffs;
	Vec4 m_tint_and_intensity;
	Int32 m_enabled;
	std::array<Int32, 3> m_padding;
};

static_assert(sizeof(GpuEnvironmentData) == 176, "GpuEnvironmentData layout mismatch");

constexpr Uint32 SHADOW_CASCADE_COUNT = 4;

struct alignas(16) GpuShadowData {
	std::array<Mat4, SHADOW_CASCADE_COUNT> m_cascade_view_proj;
	Vec4 m_cascade_splits;
	Vec4 m_light_direction;
	Vec4 m_params;
	Int32 m_enabled;
	Int32 m_num_cascades;
	std::array<Int32, 2> m_padding;
};

static_assert(sizeof(GpuShadowData) == 320, "GpuShadowData layout mismatch");

} // namespace Aquila::Rendering
