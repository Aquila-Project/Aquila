#pragma once

//   cssName    — the CSS property name (documentation / cross-reference)
//   spMember   — the Option<T> field in StyleProperties
//   csMember   — the resolved field in ComputedStyle
//   LAYOUT     — 1 if a change forces a re-layout
//   ANIM       — 1 if the property is interpolated by transitions (T must be
//   INHERIT    — 1 if the property inherits from the parent's computed value
// in this list — they remain explicit special cases in the consumers, applied

// clang-format off
#define AQ_STYLE_PROPERTY_LIST \
	AQ_STYLE_PROP("background-color",   background_color,  background_color,  0, 1, 0) \
	AQ_STYLE_PROP("border-color",       border_color,      border_color,      0, 1, 0) \
	AQ_STYLE_PROP("border-width",       border_width,      border_width,      0, 1, 0) \
	AQ_STYLE_PROP("border-radius",      border_radius,     border_radius,     0, 1, 0) \
	AQ_STYLE_PROP("border-style",       border_style,      border_style,      0, 0, 0) \
	AQ_STYLE_PROP("cursor",             cursor,           cursor,           0, 0, 0) \
	AQ_STYLE_PROP("opacity",            opacity,          opacity,          0, 1, 0) \
	AQ_STYLE_PROP("overflow",           overflow,         overflow,         1, 0, 0) \
	AQ_STYLE_PROP("display",            display,          display,          1, 0, 0) \
	AQ_STYLE_PROP("width",              width,            width,            1, 0, 0) \
	AQ_STYLE_PROP("height",             height,           height,           1, 0, 0) \
	AQ_STYLE_PROP("min-width",          min_width,         min_width,         1, 0, 0) \
	AQ_STYLE_PROP("max-width",          max_width,         max_width,         1, 0, 0) \
	AQ_STYLE_PROP("min-height",         min_height,        min_height,        1, 0, 0) \
	AQ_STYLE_PROP("max-height",         max_height,        max_height,        1, 0, 0) \
	AQ_STYLE_PROP("padding",            padding,          padding,          1, 0, 0) \
	AQ_STYLE_PROP("gap",                gap,              gap,              1, 0, 0) \
	AQ_STYLE_PROP("aspect-ratio",       aspect_ratio,      aspect_ratio,      1, 0, 0) \
	AQ_STYLE_PROP("flex-direction",     flex_direction,    flex_direction,    1, 0, 0) \
	AQ_STYLE_PROP("justify-content",    justify_content,   justify,          1, 0, 0) \
	AQ_STYLE_PROP("align-items",        align_items,       align,            1, 0, 0) \
	AQ_STYLE_PROP("flex-wrap",          flex_wrap,         wrap,             1, 0, 0) \
	AQ_STYLE_PROP("flex-grow",          flex_grow,         flex_grow,         1, 0, 0) \
	AQ_STYLE_PROP("position",           position,         position,         1, 0, 0) \
	AQ_STYLE_PROP("top",                top,              top,              1, 0, 0) \
	AQ_STYLE_PROP("right",              right,            right,            1, 0, 0) \
	AQ_STYLE_PROP("bottom",             bottom,           bottom,           1, 0, 0) \
	AQ_STYLE_PROP("left",               left,             left,             1, 0, 0) \
	AQ_STYLE_PROP("z-index",            z_index,           z_index,           1, 0, 0) \
	AQ_STYLE_PROP("color",              color,            color,            0, 1, 1) \
	AQ_STYLE_PROP("accent-color",       accent_color,      accent_color,      0, 0, 0) \
	AQ_STYLE_PROP("selection-color",    selection_color,   selection_color,   0, 0, 0) \
	AQ_STYLE_PROP("placeholder-color",  placeholder_color, placeholder_color, 0, 0, 0) \
	AQ_STYLE_PROP("font-size",          font_size,         font_size,         1, 0, 1) \
	AQ_STYLE_PROP("font-family",        font_family,       font_family,       1, 0, 1) \
	AQ_STYLE_PROP("text-align",         text_align,        text_align,        0, 0, 0) \
	AQ_STYLE_PROP("white-space",        white_space,       white_space,       1, 0, 1) \
	AQ_STYLE_PROP("text-overflow",      text_overflow,     text_overflow,     0, 0, 0) \
	AQ_STYLE_PROP("box-shadow",         box_shadows,       box_shadows,       0, 0, 0) \
	AQ_STYLE_PROP("transition-duration",transition_duration,transition_duration,0,0,0) \
	AQ_STYLE_PROP("transition-easing",  transition_easing, transition_easing, 0, 0, 0)
// clang-format on

#define AQ_STYLE_WHEN_CAT_(a, b) a##b
#define AQ_STYLE_WHEN_CAT(a, b) AQ_STYLE_WHEN_CAT_(a, b)
#define AQ_STYLE_WHEN_0(...)
#define AQ_STYLE_WHEN_1(...) __VA_ARGS__
#define AQ_STYLE_WHEN(flag, ...) AQ_STYLE_WHEN_CAT(AQ_STYLE_WHEN_, flag)(__VA_ARGS__)
