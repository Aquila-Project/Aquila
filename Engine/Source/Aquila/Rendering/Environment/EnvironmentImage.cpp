#include "Aquila/Rendering/Environment/EnvironmentImage.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Rendering/Environment/SphericalHarmonics.h"

#include <glm/gtc/packing.hpp>

namespace Aquila::Rendering {

using SceneManagement::Components::SHCoefficients;

namespace {

constexpr F32 K_HALF_MAX = 65504.F;
constexpr Uint32 K_SH_MAX_WIDTH = 512;

struct FloatLevel {
	Uint32 width = 0;
	Uint32 height = 0;
	std::vector<F32> texels;
};

F32 finite_radiance(F32 value) {
	return value > 0.F && value <= std::numeric_limits<F32>::max() ? value : 0.F;
}

FloatLevel downsample(const FloatLevel &src) {
	FloatLevel dst{ .width = std::max(1U, src.width / 2), .height = std::max(1U, src.height / 2), .texels = {} };
	dst.texels.resize(static_cast<Usize>(dst.width) * dst.height * 4);
	const auto texel = [&src](Uint32 x, Uint32 y) {
		return &src.texels[((static_cast<Usize>(std::min(y, src.height - 1)) * src.width) +
							std::min(x, src.width - 1)) *
						   4];
	};
	for (Uint32 y = 0; y < dst.height; ++y) {
		for (Uint32 x = 0; x < dst.width; ++x) {
			const F32 *a = texel(x * 2, y * 2);
			const F32 *b = texel((x * 2) + 1, y * 2);
			const F32 *c = texel(x * 2, (y * 2) + 1);
			const F32 *d = texel((x * 2) + 1, (y * 2) + 1);
			F32 *out = &dst.texels[((static_cast<Usize>(y) * dst.width) + x) * 4];
			for (Uint32 channel = 0; channel < 4; ++channel) {
				out[channel] = 0.25F * (a[channel] + b[channel] + c[channel] + d[channel]);
			}
		}
	}
	return dst;
}

std::vector<Uint8> to_half(const FloatLevel &level) {
	std::vector<Uint16> halves(level.texels.size());
	for (Usize i = 0; i < halves.size(); ++i) {
		halves[i] = glm::packHalf1x16(Math::min(level.texels[i], K_HALF_MAX));
	}
	std::vector<Uint8> bytes(halves.size() * sizeof(Uint16));
	std::memcpy(bytes.data(), halves.data(), bytes.size());
	return bytes;
}

}

EnvironmentImage EnvironmentImage::build(const F32 *rgba, Uint32 width, Uint32 height) {
	EnvironmentImage image;
	if (rgba == nullptr || width == 0 || height == 0) {
		return image;
	}

	FloatLevel level{ .width = width, .height = height, .texels = {} };
	level.texels.resize(static_cast<Usize>(width) * height * 4);
	for (Usize i = 0; i < level.texels.size(); ++i) {
		level.texels[i] = i % 4 == 3 ? 1.F : finite_radiance(rgba[i]);
	}

	bool baked_sh = false;
	while (true) {
		if (level.width <= K_MAX_WIDTH) {
			if (image.mips.levels.empty()) {
				image.mips.width = level.width;
				image.mips.height = level.height;
			}
			image.mips.levels.push_back(to_half(level));
		}
		if (!baked_sh && level.width <= K_SH_MAX_WIDTH) {
			image.irradiance = bake_equirect_sh(level.texels.data(), level.width, level.height);
			baked_sh = true;
		}
		if (level.width == 1 && level.height == 1) {
			break;
		}
		level = downsample(level);
	}
	return image;
}

SHCoefficients bake_equirect_sh(const F32 *rgba, Uint32 width, Uint32 height) {
	SHCoefficients sh;
	if (rgba == nullptr || width == 0 || height == 0) {
		return sh;
	}

	const F32 azimuth_step = Math::TAU / static_cast<F32>(width);
	const F32 elevation_step = Math::PI / static_cast<F32>(height);
	for (Uint32 y = 0; y < height; ++y) {
		const F32 elevation = Math::HALF_PI - ((static_cast<F32>(y) + 0.5F) * elevation_step);
		const F32 cos_elevation = Math::cos(elevation);
		const F32 sin_elevation = Math::sin(elevation);
		const F32 solid_angle = cos_elevation * azimuth_step * elevation_step;
		for (Uint32 x = 0; x < width; ++x) {
			const F32 azimuth = (((static_cast<F32>(x) + 0.5F) / static_cast<F32>(width)) - 0.5F) * Math::TAU;
			const Vec3 dir(cos_elevation * Math::sin(azimuth), sin_elevation, cos_elevation * Math::cos(azimuth));
			const F32 *texel = &rgba[((static_cast<Usize>(y) * width) + x) * 4];
			const Vec3 radiance =
				Vec3(finite_radiance(texel[0]), finite_radiance(texel[1]), finite_radiance(texel[2])) * solid_angle;
			const std::array<F32, 9> basis = SH::basis(dir);
			for (Usize i = 0; i < basis.size(); ++i) {
				sh.coeffs[i] += radiance * basis[i];
			}
		}
	}
	SH::convolve_cosine_lobe(sh);
	return sh;
}

}
