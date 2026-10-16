#ifndef AQUILA_COLOR_H
#define AQUILA_COLOR_H

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/Math/MathTypes.h"
#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Foundation::Color {

// RGBA clear colors
namespace RGBA {
constexpr Vec4 BLACK = { 0.0F, 0.0F, 0.0F, 1.0F };
constexpr Vec4 WHITE = { 1.0F, 1.0F, 1.0F, 1.0F };
constexpr Vec4 RED = { 1.0F, 0.0F, 0.0F, 1.0F };
constexpr Vec4 GREEN = { 0.0F, 1.0F, 0.0F, 1.0F };
constexpr Vec4 BLUE = { 0.0F, 0.0F, 1.0F, 1.0F };
constexpr Vec4 YELLOW = { 1.0F, 1.0F, 0.0F, 1.0F };
constexpr Vec4 CYAN = { 0.0F, 1.0F, 1.0F, 1.0F };
constexpr Vec4 MAGENTA = { 1.0F, 0.0F, 1.0F, 1.0F };
constexpr Vec4 ORANGE = { 1.0F, 0.5F, 0.0F, 1.0F };
constexpr Vec4 GRAY = { 0.5F, 0.5F, 0.5F, 1.0F };
constexpr Vec4 LIGHT_GRAY = { 0.75F, 0.75F, 0.75F, 1.0F };
constexpr Vec4 DARK_GRAY = { 0.25F, 0.25F, 0.25F, 1.0F };
} // namespace RGBA

// RGB colors
constexpr Vec3 BLACK_V = { 0.0F, 0.0F, 0.0F };
constexpr Vec3 WHITE_V = { 1.0F, 1.0F, 1.0F };
constexpr Vec3 RED_V = { 1.0F, 0.0F, 0.0F };
constexpr Vec3 GREEN_V = { 0.0F, 1.0F, 0.0F };
constexpr Vec3 BLUE_V = { 0.0F, 0.0F, 1.0F };
constexpr Vec3 YELLOW_V = { 1.0F, 1.0F, 0.0F };
constexpr Vec3 CYAN_V = { 0.0F, 1.0F, 1.0F };
constexpr Vec3 MAGENTA_V = { 1.0F, 0.0F, 1.0F };
constexpr Vec3 ORANGE_V = { 1.0F, 0.5F, 0.0F };
constexpr Vec3 GRAY_V = { 0.5F, 0.5F, 0.5F };
constexpr Vec3 LIGHT_GRAY_V = { 0.75F, 0.75F, 0.75F };
constexpr Vec3 DARK_GRAY_V = { 0.25F, 0.25F, 0.25F };

// Console colors
constexpr const char *RESET = "\033[0m";
constexpr const char *BOLD = "\033[1m";
constexpr const char *DIM = "\033[2m";

constexpr const char *BLACK = "\033[30m";
constexpr const char *RED = "\033[31m";
constexpr const char *GREEN = "\033[32m";
constexpr const char *YELLOW = "\033[33m";
constexpr const char *BLUE = "\033[34m";
constexpr const char *MAGENTA = "\033[35m";
constexpr const char *CYAN = "\033[36m";
constexpr const char *WHITE = "\033[37m";

constexpr const char *BRIGHT_BLACK = "\033[90m";
constexpr const char *BRIGHT_RED = "\033[91m";
constexpr const char *BRIGHT_GREEN = "\033[92m";
constexpr const char *BRIGHT_YELLOW = "\033[93m";
constexpr const char *BRIGHT_BLUE = "\033[94m";
constexpr const char *BRIGHT_MAGENTA = "\033[95m";
constexpr const char *BRIGHT_CYAN = "\033[96m";
constexpr const char *BRIGHT_WHITE = "\033[97m";

// HSV <-> RGB conversions. All values in [0, 1].

inline Vec3 hsv_to_rgb(float h, float s, float v) {
	if (s < Math::EPSILON) {
		return { v, v, v };
	}
	h = std::fmod(h, 1.F) * 6.F;
	const int i = static_cast<int>(h);
	const float f = h - static_cast<float>(i);
	const float p = v * (1.F - s);
	const float q = v * (1.F - (s * f));
	const float t = v * (1.F - (s * (1.F - f)));
	switch (i % 6) {
	case 0:
		return { v, t, p };
	case 1:
		return { q, v, p };
	case 2:
		return { p, v, t };
	case 3:
		return { p, q, v };
	case 4:
		return { t, p, v };
	default:
		return { v, p, q };
	}
}

inline Vec3 rgb_to_hsv(float r, float g, float b) {
	const float mx = std::max({ r, g, b });
	const float mn = std::min({ r, g, b });
	const float d = mx - mn;
	float h = 0.F;
	const float s = (mx > Math::EPSILON) ? d / mx : 0.F;
	if (d > Math::EPSILON) {
		if (mx == r) {
			h = ((g - b) / d / 6.F) + (g < b ? 1.F : 0.F);
		} else if (mx == g) {
			h = ((b - r) / d / 6.F) + (1.F / 3.F);
		} else {
			h = ((r - g) / d / 6.F) + (2.F / 3.F);
		}
	}
	return { h, s, mx };
}

// Texture pixel data generators. All return RGBA8 data (w * h * 4 bytes).

inline std::vector<Uint8> gen_sv(float hue, Uint32 w, Uint32 h) {
	std::vector<Uint8> px(w * h * 4);
	for (Uint32 y = 0; y < h; ++y) {
		const float v = 1.F - (y / static_cast<float>(h - 1));
		for (Uint32 x = 0; x < w; ++x) {
			const float s = x / static_cast<float>(w - 1);
			const Vec3 rgb = hsv_to_rgb(hue, s, v);
			const size_t i = ((static_cast<size_t>(y) * w) + x) * 4;
			px[i + 0] = static_cast<Uint8>(rgb.r * 255.F);
			px[i + 1] = static_cast<Uint8>(rgb.g * 255.F);
			px[i + 2] = static_cast<Uint8>(rgb.b * 255.F);
			px[i + 3] = 255;
		}
	}
	return px;
}

inline std::vector<Uint8> gen_hue(Uint32 w, Uint32 h) {
	std::vector<Uint8> px(w * h * 4);
	for (Uint32 y = 0; y < h; ++y) {
		for (Uint32 x = 0; x < w; ++x) {
			const Vec3 rgb = hsv_to_rgb(x / static_cast<float>(w - 1), 1.F, 1.F);
			const size_t i = ((static_cast<size_t>(y) * w) + x) * 4;
			px[i + 0] = static_cast<Uint8>(rgb.r * 255.F);
			px[i + 1] = static_cast<Uint8>(rgb.g * 255.F);
			px[i + 2] = static_cast<Uint8>(rgb.b * 255.F);
			px[i + 3] = 255;
		}
	}
	return px;
}

inline std::vector<Uint8> gen_channel_grad(Vec4 color, float h, float s, float v, int ch, bool is_hsv, Uint32 w,
										   Uint32 tex_h) {
	std::vector<Uint8> px(w * tex_h * 4);
	for (Uint32 x = 0; x < w; ++x) {
		const float t = x / static_cast<float>(w - 1);
		for (Uint32 y = 0; y < tex_h; ++y) {
			const size_t i = ((static_cast<size_t>(y) * w) + x) * 4;
			if (ch == 3) {
				const float bg = ((x / 6) + (y / 6)) % 2 == 0 ? 0.75F : 0.5F;
				px[i + 0] = static_cast<Uint8>(((color.r * t) + (bg * (1.F - t))) * 255.F);
				px[i + 1] = static_cast<Uint8>(((color.g * t) + (bg * (1.F - t))) * 255.F);
				px[i + 2] = static_cast<Uint8>(((color.b * t) + (bg * (1.F - t))) * 255.F);
				px[i + 3] = 255;
			} else if (!is_hsv) {
				float cr = color.r;
				float cg = color.g;
				float cb = color.b;
				switch (ch) {
				case 0:
					cr = t;
					break;
				case 1:
					cg = t;
					break;
				case 2:
					cb = t;
					break;
				}
				px[i + 0] = static_cast<Uint8>(cr * 255.F);
				px[i + 1] = static_cast<Uint8>(cg * 255.F);
				px[i + 2] = static_cast<Uint8>(cb * 255.F);
				px[i + 3] = 255;
			} else {
				float ch_h = h;
				float ch_s = s;
				float ch_v = v;
				switch (ch) {
				case 0:
					ch_h = t;
					break;
				case 1:
					ch_s = t;
					break;
				case 2:
					ch_v = t;
					break;
				}
				const Vec3 rgb = hsv_to_rgb(ch_h, ch_s, ch_v);
				px[i + 0] = static_cast<Uint8>(rgb.r * 255.F);
				px[i + 1] = static_cast<Uint8>(rgb.g * 255.F);
				px[i + 2] = static_cast<Uint8>(rgb.b * 255.F);
				px[i + 3] = 255;
			}
		}
	}
	return px;
}

inline std::vector<Uint8> gen_alpha(Vec3 rgb, Uint32 w, Uint32 h) {
	std::vector<Uint8> px(w * h * 4);
	for (Uint32 y = 0; y < h; ++y) {
		for (Uint32 x = 0; x < w; ++x) {
			const float a = x / static_cast<float>(w - 1);
			const float bg = ((x / 6) + (y / 6)) % 2 == 0 ? 0.75F : 0.5F;
			const size_t i = ((static_cast<size_t>(y) * w) + x) * 4;
			px[i + 0] = static_cast<Uint8>(((rgb.r * a) + (bg * (1.F - a))) * 255.F);
			px[i + 1] = static_cast<Uint8>(((rgb.g * a) + (bg * (1.F - a))) * 255.F);
			px[i + 2] = static_cast<Uint8>(((rgb.b * a) + (bg * (1.F - a))) * 255.F);
			px[i + 3] = 255;
		}
	}
	return px;
}

} // namespace Aquila::Foundation::Color

#endif
