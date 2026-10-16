#include "Aquila/Rendering/Environment/HosekWilkieSky.h"
#include "Aquila/Foundation/Math/Math.h"

extern "C" {
#include "hosek/ArHosekSkyModel.h"
}

namespace Aquila::Rendering {

using SceneManagement::Components::SHCoefficients;

namespace {

F32 radiance_internal(const Vec4 &a, const Vec4 &b, F32 c8, F32 theta, F32 gamma) {
	const F32 cos_gamma = Math::cos(gamma);
	const F32 cos_theta = Math::cos(theta);
	const F32 exp_m = std::exp(b.x * gamma);
	const F32 ray_m = cos_gamma * cos_gamma;
	const F32 mie_m = (1.0F + cos_gamma * cos_gamma) / Math::pow(1.0F + c8 * c8 - 2.0F * c8 * cos_gamma, 1.5F);
	const F32 zenith = Math::sqrt(Math::max(cos_theta, 0.0F));

	return (1.0F + a.x * std::exp(a.y / (cos_theta + 0.01F))) *
		   (a.z + a.w * exp_m + b.y * ray_m + b.z * mie_m + b.w * zenith);
}

void evaluate_sh_basis(const Vec3 &dir, std::array<F32, 9> &basis) {
	const F32 c0 = 0.282095F;
	const F32 c1 = 0.488603F;
	const F32 c2 = 1.092548F;
	const F32 c3 = 0.315392F;
	const F32 c4 = 0.546274F;

	basis[0] = c0;
	basis[1] = -c1 * dir.y;
	basis[2] = c1 * dir.z;
	basis[3] = -c1 * dir.x;
	basis[4] = c2 * dir.x * dir.y;
	basis[5] = -c2 * dir.y * dir.z;
	basis[6] = c3 * (3.0F * dir.z * dir.z - 1.0F);
	basis[7] = -c2 * dir.x * dir.z;
	basis[8] = c4 * (dir.x * dir.x - dir.y * dir.y);
}

} // namespace

GpuSkyData HosekWilkieSky::build(F32 sun_elevation_rad, F32 turbidity, Vec3 albedo, Vec3 sun_direction) {
	GpuSkyData data{};

	const F32 elevation = Math::clamp(sun_elevation_rad, Math::radians(0.5F), Math::HALF_PI);
	const F32 turb = Math::clamp(turbidity, 1.0F, 10.0F);

	for (int channel = 0; channel < 3; ++channel) {
		ArHosekSkyModelState *state =
			arhosek_rgb_skymodelstate_alloc_init(turb, Math::clamp(albedo[channel], 0.0F, 1.0F), elevation);

		data.config_a[channel] = Vec4(static_cast<F32>(state->configs[channel][0]),
									  static_cast<F32>(state->configs[channel][1]),
									  static_cast<F32>(state->configs[channel][2]),
									  static_cast<F32>(state->configs[channel][3]));
		data.config_b[channel] = Vec4(static_cast<F32>(state->configs[channel][4]),
									  static_cast<F32>(state->configs[channel][5]),
									  static_cast<F32>(state->configs[channel][6]),
									  static_cast<F32>(state->configs[channel][7]));
		data.config_c[channel] = Vec4(static_cast<F32>(state->configs[channel][8]),
									  static_cast<F32>(state->radiances[channel]), 0.0F, 0.0F);

		arhosekskymodelstate_free(state);
	}

	data.sun_direction = Vec4(Math::normalize(sun_direction), 0.0F);
	data.tint_intensity = Vec4(1.0F);
	data.ground_albedo = Vec4(albedo, 0.0F);
	return data;
}

Vec3 HosekWilkieSky::evaluate(const GpuSkyData &data, Vec3 direction) {
	const Vec3 dir = Math::normalize(direction);
	const Vec3 sun = Vec3(data.sun_direction);

	const F32 theta = std::acos(Math::clamp(dir.y, 0.0F, 1.0F));
	const F32 gamma = std::acos(Math::clamp(Math::dot(dir, sun), -1.0F, 1.0F));

	Vec3 result(0.0F);
	for (int channel = 0; channel < 3; ++channel) {
		result[channel] = radiance_internal(data.config_a[channel], data.config_b[channel],
											data.config_c[channel].x, theta, gamma) *
						  data.config_c[channel].y;
	}

	const F32 ground = dir.y < 0.0F ? 0.25F : 1.0F;
	return Math::max(result * kSkyBaseExposure * ground, Vec3(0.0F));
}

SHCoefficients HosekWilkieSky::bake_sh(const GpuSkyData &data, int samples) {
	SHCoefficients sh;

	std::default_random_engine rng(12345);
	std::uniform_real_distribution<F32> dist(0.0F, 1.0F);

	for (int sample = 0; sample < samples; ++sample) {
		const F32 u1 = dist(rng);
		const F32 u2 = dist(rng);
		const F32 phi = Math::TAU * u1;
		const F32 cos_theta = 2.0F * u2 - 1.0F;
		const F32 sin_theta = Math::sqrt(Math::max(0.0F, 1.0F - cos_theta * cos_theta));

		const Vec3 dir(sin_theta * Math::cos(phi), cos_theta, sin_theta * Math::sin(phi));
		const Vec3 color = evaluate(data, dir);

		std::array<F32, 9> basis{};
		evaluate_sh_basis(dir, basis);
		for (int i = 0; i < 9; ++i) {
			sh.coeffs[i] += color * basis[i];
		}
	}

	const F32 weight = 4.0F * Math::PI / static_cast<F32>(samples);
	for (auto &coefficient : sh.coeffs) {
		coefficient *= weight;
	}
	return sh;
}

} // namespace Aquila::Rendering
