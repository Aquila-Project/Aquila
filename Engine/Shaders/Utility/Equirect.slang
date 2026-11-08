#ifndef AQUILA_EQUIRECT_SLANG
#define AQUILA_EQUIRECT_SLANG

namespace Aquila::Shading {

/// Where a direction lands in an equirectangular (latitude-longitude) image: +Z is the middle of the image, +X a
/// quarter to the right of it, +Y the top row. EnvironmentImage.cpp maps pixels back to directions the same way.
float2 EquirectUV(float3 dir) {
	float u = atan2(dir.x, dir.z) * 0.15915494309 + 0.5;
	float v = 0.5 - asin(clamp(dir.y, -1.0, 1.0)) * 0.31830988618;
	return float2(u, v);
}

}

#endif
