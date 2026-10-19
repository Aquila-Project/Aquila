#ifndef AQUILA_FOUNDATION_PHOTOMETRY_H
#define AQUILA_FOUNDATION_PHOTOMETRY_H

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/PrimitiveTypes.h"

/// Physical light units, the Frostbite/Filament conventions. Lights are authored in lux (directional) and lumens
/// (point, spot), shaders light with lux and candela and output luminance in nits (cd/m2), the camera exposure turns
/// nits into pixel values.
namespace Aquila::Foundation::Photometry {

/// lm/W. Turns radiance in W/(m2 sr) into luminance in nits.
constexpr F32 LUMINOUS_EFFICACY = 683.F;

/// "Sunny 16" (f/16, 1/125 s, ISO 100): a sunlit outdoor scene. Indoors is around 7, a moonlit night around -2.
constexpr F32 DAYLIGHT_EV100 = 15.F;

/// Degrees. Bigger suns give softer shadows and wider highlights, haze makes the sun look bigger.
constexpr F32 SUN_ANGULAR_DIAMETER = 0.53F;

/// EV100 of a physical camera. Shutter time in seconds.
[[nodiscard]] inline F32 ev100(F32 aperture, F32 shutter_time, F32 iso) {
	return std::log2((aperture * aperture) / shutter_time * (100.F / iso));
}

/// Pixel value per nit. Saturation based: a luminance of 1.2 * 2^ev100 nits lands on 1.0.
[[nodiscard]] inline F32 exposure(F32 ev100) {
	return 1.F / (1.2F * std::exp2(ev100));
}

/// The EV100 a light meter picks for this average luminance (reflected meter, K = 12.5).
[[nodiscard]] inline F32 ev100_from_luminance(F32 average_nits) {
	return std::log2(Math::max(average_nits, 1e-6F) * (100.F / 12.5F));
}

/// Linear sRGB color of a black body at `kelvin` with a luminance of 1, so it only tints. 6500 K is white,
/// a candle is around 1900 K, a tungsten bulb 2700 K, overcast daylight 7000 K. Clamped to 1667..25000 K.
[[nodiscard]] inline Vec3 blackbody(F32 kelvin) {
	const F32 t = Math::clamp(kelvin, 1667.F, 25000.F);
	const F32 t2 = t * t;
	const F32 t3 = t2 * t;
	const F32 x = t <= 4000.F ? -0.2661239e9F / t3 - 0.2343589e6F / t2 + 0.8776956e3F / t + 0.179910F
							  : -3.0258469e9F / t3 + 2.1070379e6F / t2 + 0.2226347e3F / t + 0.240390F;
	const F32 x2 = x * x;
	const F32 x3 = x2 * x;
	F32 y = 0.F;
	if (t <= 2222.F) {
		y = -1.1063814F * x3 - 1.34811020F * x2 + 2.18555832F * x - 0.20219683F;
	} else if (t <= 4000.F) {
		y = -0.9549476F * x3 - 1.37418593F * x2 + 2.09137015F * x - 0.16748867F;
	} else {
		y = 3.0817580F * x3 - 5.87338670F * x2 + 3.75112997F * x - 0.37001483F;
	}

	const F32 big_x = x / y;
	const F32 big_z = (1.F - x - y) / y;
	const Vec3 rgb = Math::max(Vec3(3.2404542F * big_x - 1.5371385F - 0.4985314F * big_z,
									-0.9692660F * big_x + 1.8760108F + 0.0415560F * big_z,
									0.0556434F * big_x - 0.2040259F + 1.0572252F * big_z),
							   Vec3(0.F));
	const F32 luminance = Math::dot(rgb, Vec3(0.2126F, 0.7152F, 0.0722F));
	return luminance > 0.F ? rgb / luminance : Vec3(1.F);
}

/// Candela of a light that sends its lumens evenly in every direction.
[[nodiscard]] inline F32 point_intensity(F32 lumens) {
	return lumens / (2.F * Math::TAU);
}

/// Candela of a spot that sends all its lumens into its cone, so narrowing the cone makes it brighter, like a
/// flashlight's reflector. The half angle is in radians.
[[nodiscard]] inline F32 spot_intensity(F32 lumens, F32 outer_half_angle) {
	const F32 solid_angle = Math::TAU * (1.F - Math::cos(outer_half_angle));
	return lumens / Math::max(solid_angle, 1e-6F);
}

/// Candela of a glowing surface seen as one point light. A diffuse emitter of `area` m2 at `nits` sends out
/// pi * nits * area lumens, spread evenly that's nits * area / 4. Exact for a sphere (nits * pi * r^2), the average
/// over all directions for any convex shape.
[[nodiscard]] inline F32 emitter_intensity(F32 nits, F32 area) {
	return nits * area * 0.25F;
}

/// Radius of the sphere with the same area, the light's source radius when a glowing mesh lights the scene.
[[nodiscard]] inline F32 emitter_radius(F32 area) {
	return Math::sqrt(Math::max(area, 0.F) / (2.F * Math::TAU));
}

}

#endif
