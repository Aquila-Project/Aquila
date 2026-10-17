#pragma once

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::UI {

enum class LengthUnit : Uint8 { Pixel, Percent, Auto, Grow, Vw, Vh };

enum class FlexDirection : Uint8 { Row, Column, RowReverse, ColumnReverse };
enum class JustifyContent : Uint8 { Start, End, Center };
enum class AlignItems : Uint8 { Start, End, Center, Stretch };
enum class FlexWrap : Uint8 { NoWrap, Wrap };

enum class Display : Uint8 { Flex, None };
enum class Overflow : Uint8 { Visible, Hidden, Scroll };
enum class Position : Uint8 { Static, Relative, Absolute };
enum class BorderStyle : Uint8 { Solid, Dashed, Dotted };

enum class TransitionEasing : Uint8 { Linear, Ease, EaseIn, EaseOut, EaseInOut };

struct BoxShadow {
	Vec2 offset = { 0.F, 0.F };
	float blur = 0.F;
	float spread = 0.F;
	Vec4 color = { 0.F, 0.F, 0.F, 0.75F };
	bool inset = false;

	bool operator==(const BoxShadow &) const = default;
};

enum class TextAlign : Uint8 { Left, Center, Right };

enum class WhiteSpace : Uint8 { Normal, Nowrap };

enum class FloatingAttachTo : Uint8 { Parent, Root };
enum class FloatingAttachPoint : Uint8 {
	LeftTop,
	LeftCenter,
	LeftBottom,
	CenterTop,
	Center,
	CenterBottom,
	RightTop,
	RightCenter,
	RightBottom,
};

/// Puts a view outside the normal flow, pinned to its parent or the root (popups, tooltips).
/// `element_point` of the view sits on `parent_point` of the target, then `offset` moves it.
struct FloatingConfig {
	Vec2 offset = {};
	int16_t z_index = 10;
	FloatingAttachTo attach_to = FloatingAttachTo::Parent;
	FloatingAttachPoint element_point = FloatingAttachPoint::LeftTop;
	FloatingAttachPoint parent_point = FloatingAttachPoint::LeftBottom;
	bool operator==(const FloatingConfig &) const = default;
};

enum class FontSize : Uint8 {
	Tiny = 9,
	XSmall = 11,
	Small = 13,
	Body = 16,
	BodyLarge = 18,
	Subtitle = 20,
	Heading = 24,
	HeadingLarge = 28,
	Title = 32,
	TitleLarge = 40,
	Display = 48,
	DisplayLarge = 64,
};

constexpr float font_size_to_pixels(FontSize size) {
	return static_cast<float>(static_cast<Uint8>(size));
}

} // namespace Aquila::UI
