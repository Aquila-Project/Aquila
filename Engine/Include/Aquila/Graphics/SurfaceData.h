#pragma once
#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Graphics {

/// One entry of the per-frame material buffer. Has to match the shader side struct byte for byte.
struct alignas(16) GpuSurfaceData {
	Vec4 albedo{ 1.F, 1.F, 1.F, 1.F };
	Vec4 emissive{ 0.F, 0.F, 0.F, 0.F };
	F32 metallic = 0.F;
	F32 roughness = 0.5F;
	F32 normal_strength = 1.F;
	F32 ao_strength = 1.F;
	Vec4 extra0{ 0.F };
	Vec4 extra1{ 0.F };
};
static_assert(sizeof(GpuSurfaceData) == 80, "GpuSurfaceData must be 80 bytes (std430)");

} // namespace Aquila::Graphics
