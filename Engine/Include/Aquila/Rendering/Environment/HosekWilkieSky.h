#pragma once
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Scene/Components/SkyLightComponent.h"

namespace Aquila::Rendering {

struct alignas(16) GpuSkyData {
	Vec4 config_a[3];
	Vec4 config_b[3];
	Vec4 config_c[3];
	Vec4 sun_direction;
	Vec4 tint_intensity;
	Vec4 ground_albedo;
};

class HosekWilkieSky {
  public:
	/// Hosek-Wilkie coefficients for these conditions, the shader evaluates the sky per pixel from them.
	static GpuSkyData build(F32 sun_elevation_rad, F32 turbidity, Vec3 albedo, Vec3 sun_direction);
	/// Sky luminance in nits.
	static Vec3 evaluate(const GpuSkyData &data, Vec3 direction);
	/// Projects the sky onto 9 SH coefficients for the ambient light, already convolved with the cosine lobe and
	/// divided by pi: evaluated at a normal they give irradiance / pi, so albedo times that is the diffuse radiance.
	/// Not cheap, bake only when the sky changes.
	static SceneManagement::Components::SHCoefficients bake_sh(const GpuSkyData &data, int samples = 8192);
};

} // namespace Aquila::Rendering
