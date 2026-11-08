#ifndef AQUILA_CUBE_MAP_SLANG
#define AQUILA_CUBE_MAP_SLANG

namespace Aquila::Shading {

/// The direction a pixel of a cube face stands for: the GPU's own face lookup run backwards (Vulkan spec, "Cube Map
/// Face Selection and Transformations"). Faces go +X -X +Y -Y +Z -Z. `uv` is 0..1 across the face with v going down
/// the image, which is what FullscreenTexCoord gives.
float3 CubeFaceDirection(uint face, float2 uv) {
	float u = uv.x * 2.0 - 1.0;
	float v = uv.y * 2.0 - 1.0;
	switch (face) {
	case 0:
		return normalize(float3(1.0, -v, -u));
	case 1:
		return normalize(float3(-1.0, -v, u));
	case 2:
		return normalize(float3(u, 1.0, v));
	case 3:
		return normalize(float3(u, -1.0, -v));
	case 4:
		return normalize(float3(u, -v, 1.0));
	default:
		return normalize(float3(-u, -v, -1.0));
	}
}

}

#endif
