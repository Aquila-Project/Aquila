#pragma once

#include "Aquila/UI/Style/StyleDefaults.h"
#include "Aquila/UI/Style/StyleLength.h"

namespace Aquila::UI {

struct ComputedStyle {
	Vec4 background_color = StyleDefaults::BACKGROUND_COLOR;
	Vec4 border_color = StyleDefaults::BORDER_COLOR;
	Vec4 border_radius = StyleDefaults::BORDER_RADIUS;
	F32 border_width = StyleDefaults::BORDER_WIDTH;
	BorderStyle border_style = StyleDefaults::BORDER_STYLE;
	Platform::CursorType cursor = StyleDefaults::CURSOR;
	F32 opacity = StyleDefaults::OPACITY;

	StyleLength width = StyleDefaults::WIDTH;
	StyleLength height = StyleDefaults::HEIGHT;

	StyleLength min_width = StyleDefaults::MIN_WIDTH;
	StyleLength max_width = StyleDefaults::MAX_WIDTH;
	StyleLength min_height = StyleDefaults::MIN_HEIGHT;
	StyleLength max_height = StyleDefaults::MAX_HEIGHT;

	StyleEdges padding = StyleDefaults::PADDING;
	F32 gap = 0.F;
	F32 aspect_ratio = 0.F; // 0 = unset (width/height ratio when one axis is auto)

	FlexDirection flex_direction = StyleDefaults::FLEX_DIR;
	JustifyContent justify = StyleDefaults::JUSTIFY;
	AlignItems align = StyleDefaults::ALIGN;
	FlexWrap wrap = StyleDefaults::WRAP;
	F32 flex_grow = StyleDefaults::FLEX_GROW;

	Position position = StyleDefaults::POS;
	StyleLength top = StyleDefaults::TOP;
	StyleLength bottom = StyleDefaults::BOTTOM;
	StyleLength left = StyleDefaults::LEFT;
	StyleLength right = StyleDefaults::RIGHT;

	int z_index = StyleDefaults::Z_INDEX;
	Display display = StyleDefaults::DISP;
	Overflow overflow = StyleDefaults::OVERFLOW_DEFAULT;
	Vec4 color = StyleDefaults::COLOR;
	Vec4 accent_color = Vec4(0.F); // transparent sentinel; if alpha==0 widgets fall back to style.color
	Vec4 selection_color = Vec4(0.F); // transparent sentinel; if alpha==0 widgets derive from style.color
	Vec4 placeholder_color = Vec4(0.F); // transparent sentinel; if alpha==0 widgets derive from style.color

	F32 font_size = StyleDefaults::FONT_SIZE; // 0 = unset / inherit from parent
	std::string font_family; // "" = unset / inherit from parent
	TextAlign text_align = TextAlign::Left;
	WhiteSpace white_space = StyleDefaults::WHITE_SPACE;

	std::vector<BoxShadow> box_shadows;

	F32 transition_duration = StyleDefaults::TRANSITION_DURATION; // ms
	TransitionEasing transition_easing = StyleDefaults::TRANSITION_EASE;

	bool operator==(const ComputedStyle &b) const = default;

	[[nodiscard]] Vec4 effective_accent_color() const { return accent_color.a > 0.F ? accent_color : color; }
	[[nodiscard]] Vec4 effective_selection_color() const {
		return selection_color.a > 0.F ? selection_color : Vec4(color.r, color.g, color.b, 0.3F);
	}
	[[nodiscard]] Vec4 effective_placeholder_color() const {
		return placeholder_color.a > 0.F ? placeholder_color : Vec4(color.r, color.g, color.b, color.a * 0.45F);
	}
};

} // namespace Aquila::UI
