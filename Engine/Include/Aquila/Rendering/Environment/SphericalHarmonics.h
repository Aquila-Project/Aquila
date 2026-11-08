#ifndef AQUILA_RENDERING_SPHERICAL_HARMONICS_H
#define AQUILA_RENDERING_SPHERICAL_HARMONICS_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Scene/Components/SkyLightComponent.h"

namespace Aquila::Rendering::SH {

/// Real SH basis up to band 2, in the order and with the signs EnvironmentData.slang's EvaluateSH() reads.
[[nodiscard]] inline std::array<F32, 9> basis(const Vec3 &dir) {
	const F32 c0 = 0.282095F;
	const F32 c1 = 0.488603F;
	const F32 c2 = 1.092548F;
	const F32 c3 = 0.315392F;
	const F32 c4 = 0.546274F;
	return { c0,
			 -c1 * dir.y,
			 c1 * dir.z,
			 -c1 * dir.x,
			 c2 * dir.x * dir.y,
			 -c2 * dir.y * dir.z,
			 c3 * (3.0F * dir.z * dir.z - 1.0F),
			 -c2 * dir.x * dir.z,
			 c4 * (dir.x * dir.x - dir.y * dir.y) };
}

/// Radiance coefficients to irradiance / pi (bands times 1, 2/3, 1/4), which is what EvaluateSH() expects.
inline void convolve_cosine_lobe(SceneManagement::Components::SHCoefficients &sh) {
	constexpr std::array<F32, 9> K_COSINE_LOBE = { 1.0F,  2.0F / 3.0F, 2.0F / 3.0F, 2.0F / 3.0F, 0.25F,
												   0.25F, 0.25F,	   0.25F,		0.25F };
	for (Usize i = 0; i < K_COSINE_LOBE.size(); ++i) {
		sh.coeffs[i] *= K_COSINE_LOBE[i];
	}
}

}

#endif
