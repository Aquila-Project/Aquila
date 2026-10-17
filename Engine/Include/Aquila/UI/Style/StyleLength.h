#pragma once

#include "Aquila/UI/Style/StyleTypes.h"

namespace Aquila::UI {

struct StyleLength {
	LengthUnit unit = LengthUnit::Auto;
	float value = 0.0F;

	constexpr static StyleLength pixel(float pixels) { return { .unit = LengthUnit::Pixel, .value = pixels }; }
	constexpr static StyleLength percent(float percent) { return { .unit = LengthUnit::Percent, .value = percent }; }
	constexpr static StyleLength Auto() { return { .unit = LengthUnit::Auto, .value = 0.F }; }
	/// Takes the space that's left, like flex-grow.
	constexpr static StyleLength grow() { return { .unit = LengthUnit::Grow, .value = 0.F }; }
	constexpr static StyleLength vw(float viewport_width_percent) { return { .unit = LengthUnit::Vw, .value = viewport_width_percent }; }
	constexpr static StyleLength vh(float viewport_height_percent) { return { .unit = LengthUnit::Vh, .value = viewport_height_percent }; }

	[[nodiscard]] bool is_auto() const { return unit == LengthUnit::Auto; }
	[[nodiscard]] bool is_grow() const { return unit == LengthUnit::Grow; }
	[[nodiscard]] bool is_pixel() const { return unit == LengthUnit::Pixel; }
	[[nodiscard]] bool is_percent() const { return unit == LengthUnit::Percent; }
	[[nodiscard]] bool is_viewport() const { return unit == LengthUnit::Vw || unit == LengthUnit::Vh; }

	/// Pixels and percent only, every other unit gives 0. Layout handles the rest.
	[[nodiscard]] float resolve(float parent_size) const {
		switch (unit) {
		case LengthUnit::Pixel:
			return value;
		case LengthUnit::Percent:
			return parent_size * (value / 100.0F);
		default:
			return 0.F;
		}
	}

	bool operator==(const StyleLength &other) const { return unit == other.unit && value == other.value; }
	bool operator!=(const StyleLength &other) const { return !(*this == other); }
};

struct StyleEdges {
	StyleLength top, right, bottom, left;

	constexpr static StyleEdges all(StyleLength value) {
		return { .top = value, .right = value, .bottom = value, .left = value };
	}
	constexpr static StyleEdges axes(StyleLength vertical, StyleLength horizontal) {
		return { .top = vertical, .right = horizontal, .bottom = vertical, .left = horizontal };
	}
	constexpr static StyleEdges zero() { return all(StyleLength::pixel(0.F)); }

	bool operator==(const StyleEdges &other) const {
		return top == other.top && right == other.right && bottom == other.bottom && left == other.left;
	}
};

} // namespace Aquila::UI
