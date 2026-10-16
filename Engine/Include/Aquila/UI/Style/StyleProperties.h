#pragma once

#include "Aquila/UI/Style/StyleLength.h"
#include "Aquila/Platform/Cursor.h"

namespace Aquila::UI {

struct StyleProperties {
	Option<Vec4> background_color;
	Option<Vec4> border_color;
	Option<F32> border_width;
	Option<Vec4> border_radius;
	Option<BorderStyle> border_style;
	Option<Platform::CursorType> cursor;
	Option<F32> opacity;
	Option<Overflow> overflow;
	Option<Display> display;

	Option<StyleLength> width, height;
	Option<StyleLength> min, max;
	Option<StyleLength> min_width, max_width;
	Option<StyleLength> min_height, max_height;
	Option<StyleEdges> padding;
	Option<StyleLength> padding_left, padding_right, padding_top, padding_bottom;
	Option<F32> gap;

	Option<F32> aspect_ratio;

	Option<FlexDirection> flex_direction;
	Option<JustifyContent> justify_content;
	Option<AlignItems> align_items;
	Option<F32> flex_grow;
	Option<FlexWrap> flex_wrap;

	Option<Position> position;
	Option<StyleLength> top, right, bottom, left;
	Option<Int32> z_index;

	Option<Vec4> color;
	Option<Vec4> accent_color;
	Option<Vec4> selection_color;
	Option<Vec4> placeholder_color;
	Option<F32> font_size;
	Option<std::string> font_family;
	Option<TextAlign> text_align;
	Option<WhiteSpace> white_space;

	Option<std::vector<BoxShadow>> box_shadows;

	Option<F32> transition_duration; // milliseconds
	Option<TransitionEasing> transition_easing;
};

} // namespace Aquila::UI
