#include "Aquila/UI/Core/LayoutEngine.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/UI/Text/FontAtlas.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-designated-field-initializers"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#pragma clang diagnostic ignored "-Wnested-anon-types"
#include "clay.h"
#pragma clang diagnostic pop

namespace Aquila::UI::Core {

static Clay_LayoutAlignmentX justify_to_align_x(JustifyContent justify) {
	switch (justify) {
	case JustifyContent::Center:
		return CLAY_ALIGN_X_CENTER;
	case JustifyContent::End:
		return CLAY_ALIGN_X_RIGHT;
	default:
		return CLAY_ALIGN_X_LEFT;
	}
}

static Clay_LayoutAlignmentY justify_to_align_y(JustifyContent justify) {
	switch (justify) {
	case JustifyContent::Center:
		return CLAY_ALIGN_Y_CENTER;
	case JustifyContent::End:
		return CLAY_ALIGN_Y_BOTTOM;
	default:
		return CLAY_ALIGN_Y_TOP;
	}
}

static Clay_LayoutAlignmentX align_to_align_x(AlignItems align) {
	switch (align) {
	case AlignItems::Center:
		return CLAY_ALIGN_X_CENTER;
	case AlignItems::End:
		return CLAY_ALIGN_X_RIGHT;
	default:
		return CLAY_ALIGN_X_LEFT;
	}
}

static Clay_LayoutAlignmentY align_to_align_y(AlignItems align) {
	switch (align) {
	case AlignItems::Center:
		return CLAY_ALIGN_Y_CENTER;
	case AlignItems::End:
		return CLAY_ALIGN_Y_BOTTOM;
	default:
		return CLAY_ALIGN_Y_TOP;
	}
}

static Clay_TextAlignment to_clay_text_align(TextAlign align) {
	switch (align) {
	case TextAlign::Center:
		return CLAY_TEXT_ALIGN_CENTER;
	case TextAlign::Right:
		return CLAY_TEXT_ALIGN_RIGHT;
	default:
		return CLAY_TEXT_ALIGN_LEFT;
	}
}

static constexpr F32 k_font_size_units = 100.F;

static uint16_t to_clay_font_size(F32 font_size) {
	return static_cast<uint16_t>(Math::clamp(Math::round(font_size * k_font_size_units), 0.F, 65535.F));
}

static uint16_t to_clay_font_id(const Text::FontAtlas *font) {
	static std::unordered_map<const Text::FontAtlas *, uint16_t> ids;
	const auto [it, inserted] = ids.try_emplace(font, static_cast<uint16_t>(ids.size()));
	return it->second;
}

static Clay_Dimensions measure_clay_text(Clay_StringSlice text, Clay_TextElementConfig *config, void * /*userData*/) {
	auto *font = static_cast<Text::FontAtlas *>(config->userData);
	if (font == nullptr || text.length <= 0) {
		return { 0.F, 0.F };
	}
	const std::string_view slice(text.chars, static_cast<size_t>(text.length));
	const Vec2 dims = font->measure_text(slice, static_cast<F32>(config->fontSize) / k_font_size_units);
	return { dims.x, dims.y };
}

static F32 to_absolute_px(const StyleLength &len, F32 vw_px, F32 vh_px) {
	switch (len.unit) {
	case LengthUnit::Pixel:
		return len.value;
	case LengthUnit::Vw:
		return len.value * vw_px;
	case LengthUnit::Vh:
		return len.value * vh_px;
	default:
		return 0.F;
	}
}

static Clay_SizingAxis to_c_sizing(const StyleLength &len, F32 vw_px, F32 vh_px) {
	switch (len.unit) {
	case LengthUnit::Pixel:
		return CLAY_SIZING_FIXED(len.value);
	case LengthUnit::Vw:
		return CLAY_SIZING_FIXED(len.value * vw_px);
	case LengthUnit::Vh:
		return CLAY_SIZING_FIXED(len.value * vh_px);
	case LengthUnit::Percent:
		return CLAY_SIZING_PERCENT(len.value / 100.F);
	case LengthUnit::Grow:
		return CLAY_SIZING_GROW(0);
	case LengthUnit::Auto:
	default:
		return CLAY_SIZING_FIT(0, 0);
	}
}

static Clay_Padding to_clay_padding(const StyleEdges &edges) {
	auto px = [](const StyleLength &l) -> uint16_t {
		return l.unit == LengthUnit::Pixel ? static_cast<uint16_t>(l.value) : 0u;
	};
	return { .left = px(edges.left), .right = px(edges.right), .top = px(edges.top), .bottom = px(edges.bottom) };
}

static Clay_SizingAxis to_c_sizing_axis(const StyleLength &len, const StyleLength &min_len, const StyleLength &max_len,
										F32 vw_px, F32 vh_px) {
	if (len.unit == LengthUnit::Vw || len.unit == LengthUnit::Vh) {
		F32 value = to_absolute_px(len, vw_px, vh_px);
		const F32 min_px = to_absolute_px(min_len, vw_px, vh_px);
		const F32 max_px = to_absolute_px(max_len, vw_px, vh_px);
		if (max_px > 0.F && value > max_px) {
			value = max_px;
		}
		if (min_px > 0.F && value < min_px) {
			value = min_px;
		}
		return CLAY_SIZING_FIXED(value);
	}

	Clay_SizingAxis axis = to_c_sizing(len, vw_px, vh_px);
	if (len.unit == LengthUnit::Auto || len.unit == LengthUnit::Grow) {
		const F32 min_px = to_absolute_px(min_len, vw_px, vh_px);
		const F32 max_px = to_absolute_px(max_len, vw_px, vh_px);
		if (min_px > 0.F) {
			axis.size.minMax.min = min_px;
		}
		if (max_px > 0.F) {
			axis.size.minMax.max = max_px;
		}
	}
	return axis;
}

static bool is_row(FlexDirection direction) {
	return direction == FlexDirection::Row || direction == FlexDirection::RowReverse;
}

static bool is_reverse(FlexDirection direction) {
	return direction == FlexDirection::RowReverse || direction == FlexDirection::ColumnReverse;
}

static JustifyContent main_justify(const ComputedStyle &cs) {
	if (!is_reverse(cs.flex_direction) || cs.justify == JustifyContent::Center) {
		return cs.justify;
	}
	return cs.justify == JustifyContent::Start ? JustifyContent::End : JustifyContent::Start;
}

static bool is_in_flow(const View *view) {
	return view->get_display_style().display != Display::None && !view->has_floating() &&
		view->get_display_style().position != Position::Absolute;
}

static Clay_LayoutConfig to_clay_layout(const ComputedStyle &cs, F32 vw_px, F32 vh_px) {
	const bool row = is_row(cs.flex_direction);
	const JustifyContent justify = main_justify(cs);
	const Clay_LayoutAlignmentX align_x = row ? justify_to_align_x(justify) : align_to_align_x(cs.align);
	const Clay_LayoutAlignmentY align_y = row ? align_to_align_y(cs.align) : justify_to_align_y(justify);

	Clay_LayoutConfig layout = {
		.sizing = { .width = to_c_sizing_axis(cs.width, cs.min_width, cs.max_width, vw_px, vh_px),
					.height = to_c_sizing_axis(cs.height, cs.min_height, cs.max_height, vw_px, vh_px) },
		.padding = to_clay_padding(cs.padding),
		.childGap = static_cast<uint16_t>(cs.gap),
		.childAlignment = { .x = align_x, .y = align_y },
		.layoutDirection = row ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM,
	};

	if (cs.wrap == FlexWrap::Wrap) {
		layout.childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP };
		layout.layoutDirection = row ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT;
	}

	return layout;
}

static Clay_ElementId make_element_id(View *node) {
	const std::string &view_id = node->get_id();
	if (!view_id.empty()) {
		const Clay_String s{
			.isStaticallyAllocated = false,
			.length = static_cast<Int32>(view_id.size()),
			.chars = view_id.c_str(),
		};
		return CLAY_SID(s);
	}

	const Uint32 id = node->get_stable_id();
	return Clay_ElementId{ .id = id != 0 ? id : 1u };
}

LayoutEngine::LayoutEngine(Uint32 width, Uint32 height) : m_width(width), m_height(height) {
	const Uint32 mem_size = Clay_MinMemorySize();
	m_clay_memory.resize(mem_size);
	const Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(mem_size, m_clay_memory.data());
	Clay_Context *ctx = Clay_Initialize(arena, { static_cast<F32>(width), static_cast<F32>(height) }, {});
	m_clay_ctx = ctx;
	Clay_SetCurrentContext(ctx);
	Clay_SetMeasureTextFunction(measure_clay_text, nullptr);
}

LayoutEngine::~LayoutEngine() {
	if (Clay_GetCurrentContext() == static_cast<Clay_Context *>(m_clay_ctx)) {
		Clay_SetCurrentContext(nullptr);
	}
}

void LayoutEngine::set_dimensions(Uint32 width, Uint32 height) {
	m_width = width;
	m_height = height;
	Clay_SetCurrentContext(static_cast<Clay_Context *>(m_clay_ctx));
	Clay_SetLayoutDimensions({ static_cast<F32>(width), static_cast<F32>(height) });
}

void LayoutEngine::run_layout(View *root, Vec2 mouse_pos, bool mouse_down, Vec2 scroll_delta, float delta_time) {
	Clay_SetCurrentContext(static_cast<Clay_Context *>(m_clay_ctx));
	Clay_SetLayoutDimensions({ static_cast<F32>(m_width), static_cast<F32>(m_height) });
	Clay_SetPointerState({ mouse_pos.x, mouse_pos.y }, mouse_down);
	Clay_UpdateScrollContainers(false, { scroll_delta.x, scroll_delta.y }, delta_time);
	m_size_changed = false;
	m_resized_nodes.clear();
	constexpr int k_max_wrap_passes = 3;
	for (int pass = 0; pass < k_max_wrap_passes; ++pass) {
		m_wrap_lines.clear();
		m_intrinsic_sizes.clear();
		Clay_BeginLayout();
		layout_pass(root);
		Clay_EndLayout(pass == 0 ? delta_time : 0.F);
		update_rects(root);
		if (!wrap_lines_changed() && !intrinsic_sizes_changed()) {
			break;
		}
	}
}

F32 LayoutEngine::resolve_offset(const StyleLength &len, F32 parent_size) const {
	switch (len.unit) {
	case LengthUnit::Pixel:
		return len.value;
	case LengthUnit::Percent:
		return len.value / 100.F * parent_size;
	case LengthUnit::Vw:
		return len.value * static_cast<F32>(m_width) / 100.F;
	case LengthUnit::Vh:
		return len.value * static_cast<F32>(m_height) / 100.F;
	default:
		return 0.F;
	}
}

std::vector<View *> LayoutEngine::flow_children(View *node) const {
	std::vector<View *> children;
	for (const auto &child : node->get_children()) {
		if (is_in_flow(child.get())) {
			children.push_back(child.get());
		}
	}
	if (is_reverse(node->get_display_style().flex_direction)) {
		std::ranges::reverse(children);
	}
	return children;
}

std::vector<Uint32> LayoutEngine::compute_wrap_lines(View *node) const {
	const ComputedStyle &cs = node->get_display_style();
	const bool row = is_row(cs.flex_direction);
	const std::vector<View *> children = flow_children(node);
	if (children.empty()) {
		return {};
	}

	auto pixels = [](const StyleLength &l) { return l.unit == LengthUnit::Pixel ? l.value : 0.F; };
	const Vec2 size = node->get_layout_rect().size;
	const F32 available = row ? size.x - pixels(cs.padding.left) - pixels(cs.padding.right)
							  : size.y - pixels(cs.padding.top) - pixels(cs.padding.bottom);
	if (available <= 0.F) {
		return { static_cast<Uint32>(children.size()) };
	}

	std::vector<Uint32> lines;
	Uint32 count = 0;
	F32 used = 0.F;
	for (View *child : children) {
		const ComputedStyle &child_style = child->get_display_style();
		const StyleLength &main_len = row ? child_style.width : child_style.height;
		const StyleLength &main_min = row ? child_style.min_width : child_style.min_height;
		F32 extent = row ? child->get_layout_rect().size.x : child->get_layout_rect().size.y;
		if (main_len.unit == LengthUnit::Pixel) {
			extent = main_len.value;
		} else if (main_len.unit == LengthUnit::Grow || child_style.flex_grow > 0.F) {
			extent = pixels(main_min);
		}
		const F32 needed = count == 0 ? extent : used + cs.gap + extent;
		if (count > 0 && needed > available) {
			lines.push_back(count);
			count = 0;
			used = 0.F;
		}
		used = count == 0 ? extent : used + cs.gap + extent;
		++count;
	}
	lines.push_back(count);
	return lines;
}

bool LayoutEngine::intrinsic_sizes_changed() const {
	for (const View *node : m_resized_nodes) {
		const auto it = m_intrinsic_sizes.find(node);
		if (it != m_intrinsic_sizes.end() && node->get_intrinsic_size() != it->second) {
			return true;
		}
	}
	return false;
}

bool LayoutEngine::wrap_lines_changed() const {
	for (const auto &[node, lines] : m_wrap_lines) {
		if (compute_wrap_lines(const_cast<View *>(node)) != lines) {
			return true;
		}
	}
	return false;
}

void LayoutEngine::scroll_into_view(View *target) {
	if (target == nullptr) {
		return;
	}

	View *container = nullptr;
	for (View *v = target->get_parent(); v != nullptr; v = v->get_parent()) {
		if (v->get_display_style().overflow == Overflow::Scroll) {
			container = v;
			break;
		}
	}
	if (container == nullptr) {
		return;
	}

	Clay_SetCurrentContext(static_cast<Clay_Context *>(m_clay_ctx));
	Clay_ElementId id = {};
	id.id = container->get_clay_id();
	Clay_ScrollContainerData data = Clay_GetScrollContainerData(id);
	if (!data.found || data.scrollPosition == nullptr) {
		return;
	}

	const Rect container_rect = container->get_absolute_rect();
	const Rect target_rect = target->get_absolute_rect();
	const float rel = target_rect.position.y - container_rect.position.y;
	const float view_h = container_rect.size.y;
	const float node_h = target_rect.size.y;

	float sy = data.scrollPosition->y;
	if (rel < 0.F) {
		sy -= rel;
	} else if (rel + node_h > view_h) {
		sy -= (rel + node_h - view_h);
	}

	const float max_scroll = data.contentDimensions.height - view_h;
	if (max_scroll <= 0.F) {
		sy = 0.F;
	} else if (sy < -max_scroll) {
		sy = -max_scroll;
	} else if (sy > 0.F) {
		sy = 0.F;
	}
	data.scrollPosition->y = sy;
}

void LayoutEngine::set_scroll_offset(View *target, float offset_y) {
	if (target == nullptr) {
		return;
	}

	Clay_SetCurrentContext(static_cast<Clay_Context *>(m_clay_ctx));
	Clay_ElementId id = {};
	id.id = target->get_clay_id();
	Clay_ScrollContainerData data = Clay_GetScrollContainerData(id);
	if (!data.found || data.scrollPosition == nullptr) {
		return;
	}

	const float view_h = target->get_absolute_rect().size.y;
	const float max_scroll = data.contentDimensions.height - view_h;
	float clamped = offset_y;
	if (max_scroll <= 0.F || clamped < 0.F) {
		clamped = 0.F;
	} else if (clamped > max_scroll) {
		clamped = max_scroll;
	}
	data.scrollPosition->y = -clamped;
}

void LayoutEngine::layout_pass(View *node) {
	const ComputedStyle &cs = node->get_display_style();

	if (cs.display == Display::None) {
		return;
	}

	const Clay_ElementId clay_id = make_element_id(node);
	node->set_clay_id(clay_id.id);

	ClayTextRun text_run;
	const bool is_text = node->get_clay_text_run(text_run);

	auto emit_content = [&]() {
		if (is_text) {
			text_run.font->ensure_glyphs(text_run.text);
			const Clay_String text_string{
				.isStaticallyAllocated = false,
				.length = static_cast<Int32>(text_run.text.size()),
				.chars = text_run.text.data(),
			};
			Clay_TextElementConfig text_config{};
			text_config.userData = text_run.font;
			text_config.fontId = to_clay_font_id(text_run.font);
			text_config.fontSize = to_clay_font_size(text_run.font_size);
			text_config.wrapMode = CLAY_TEXT_WRAP_WORDS;
			text_config.textAlignment = to_clay_text_align(text_run.align);
			Clay__OpenTextElement(text_string, text_config);
			return;
		}
		if (cs.wrap != FlexWrap::Wrap) {
			for (View *child : flow_children(node)) {
				layout_pass(child);
			}
		} else {
			const std::vector<View *> children = flow_children(node);
			const std::vector<Uint32> lines = compute_wrap_lines(node);
			m_wrap_lines[node] = lines;
			const bool row = is_row(cs.flex_direction);
			const JustifyContent justify = main_justify(cs);
			const Clay_LayoutConfig line_layout = {
				.sizing = { .width = row ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIT(0, 0),
							.height = row ? CLAY_SIZING_FIT(0, 0) : CLAY_SIZING_GROW(0) },
				.childGap = static_cast<uint16_t>(cs.gap),
				.childAlignment = { .x = row ? justify_to_align_x(justify) : align_to_align_x(cs.align),
									.y = row ? align_to_align_y(cs.align) : justify_to_align_y(justify) },
				.layoutDirection = row ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM,
			};
			Usize next = 0;
			for (Usize line = 0; line < lines.size(); ++line) {
				const Clay_ElementId line_id = CLAY_SIDI(CLAY_STRING("aquila-wrap-line"),
														 (node->get_stable_id() * 1024u) + static_cast<Uint32>(line));
				CLAY(line_id, { .layout = line_layout }) {
					for (Uint32 i = 0; i < lines[line] && next < children.size(); ++i) {
						layout_pass(children[next++]);
					}
				}
			}
		}
		for (const auto &child : node->get_children()) {
			if (!is_in_flow(child.get())) {
				layout_pass(child.get());
			}
		}
	};

	const F32 vw_px = static_cast<F32>(m_width) / 100.F;
	const F32 vh_px = static_cast<F32>(m_height) / 100.F;
	Clay_LayoutConfig layout = to_clay_layout(cs, vw_px, vh_px);
	if (!is_text) {
		const Vec2 intrinsic = node->get_intrinsic_size();
		m_intrinsic_sizes[node] = intrinsic;
		if (intrinsic.x >= 0.F && cs.width.unit == LengthUnit::Auto) {
			layout.sizing.width = CLAY_SIZING_FIXED(intrinsic.x);
		}
		if (intrinsic.y >= 0.F && cs.height.unit == LengthUnit::Auto) {
			layout.sizing.height = CLAY_SIZING_FIXED(intrinsic.y);
		}
	}

	View *parent = node->get_parent();
	if (parent != nullptr && cs.position != Position::Absolute && !node->has_floating()) {
		const ComputedStyle &parent_style = parent->get_display_style();
		const bool grow_main = cs.flex_grow > 0.F;
		const bool stretch = parent_style.align == AlignItems::Stretch;
		const bool parent_row = is_row(parent_style.flex_direction);
		const bool width_grows = parent_row ? grow_main : stretch;
		const bool height_grows = parent_row ? stretch : grow_main;
		auto grow_from = [&](Clay_SizingAxis current, const StyleLength &min_len, const StyleLength &max_len) {
			Clay_SizingAxis axis = to_c_sizing_axis(StyleLength::grow(), min_len, max_len, vw_px, vh_px);
			if (current.type == CLAY__SIZING_TYPE_FIXED) {
				axis.size.minMax.min = Math::max(axis.size.minMax.min, current.size.minMax.min);
				if (axis.size.minMax.max > 0.F) {
					axis.size.minMax.min = Math::min(axis.size.minMax.min, axis.size.minMax.max);
				}
			}
			return axis;
		};
		if (width_grows && cs.width.unit == LengthUnit::Auto) {
			layout.sizing.width = grow_from(layout.sizing.width, cs.min_width, cs.max_width);
		}
		if (height_grows && cs.height.unit == LengthUnit::Auto) {
			layout.sizing.height = grow_from(layout.sizing.height, cs.min_height, cs.max_height);
		}
	}

	const Clay_AspectRatioElementConfig aspect_cfg = { cs.aspect_ratio };

	if (!node->has_floating() && cs.position == Position::Absolute) {
		const Vec2 parent_size = parent != nullptr ? parent->get_layout_rect().size
												   : Vec2(static_cast<F32>(m_width), static_cast<F32>(m_height));
		const bool from_right = cs.left.unit == LengthUnit::Auto && cs.right.unit != LengthUnit::Auto;
		const bool from_bottom = cs.top.unit == LengthUnit::Auto && cs.bottom.unit != LengthUnit::Auto;
		const Clay_FloatingAttachPointType point = from_right
			? (from_bottom ? CLAY_ATTACH_POINT_RIGHT_BOTTOM : CLAY_ATTACH_POINT_RIGHT_TOP)
			: (from_bottom ? CLAY_ATTACH_POINT_LEFT_BOTTOM : CLAY_ATTACH_POINT_LEFT_TOP);
		const Clay_FloatingElementConfig float_cfg = {
			.offset = { .x = from_right ? -resolve_offset(cs.right, parent_size.x) : resolve_offset(cs.left, parent_size.x),
						.y = from_bottom ? -resolve_offset(cs.bottom, parent_size.y)
										 : resolve_offset(cs.top, parent_size.y) },
			.zIndex = static_cast<int16_t>(Math::clamp(cs.z_index, -32768, 32767)),
			.attachPoints = { .element = point, .parent = point },
			.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
			.attachTo = CLAY_ATTACH_TO_PARENT,
		};
		switch (cs.overflow) {
		case Overflow::Scroll:
			CLAY(clay_id,
				 { .layout = layout,
				   .aspectRatio = aspect_cfg,
				   .floating = float_cfg,
				   .clip = { .horizontal = false, .vertical = true, .childOffset = Clay_GetScrollOffset() } }) {
				emit_content();
			}
			break;
		case Overflow::Clip:
			CLAY(clay_id,
				 { .layout = layout,
				   .aspectRatio = aspect_cfg,
				   .floating = float_cfg,
				   .clip = { .horizontal = true, .vertical = true } }) {
				emit_content();
			}
			break;
		default:
			CLAY(clay_id, { .layout = layout, .aspectRatio = aspect_cfg, .floating = float_cfg }) {
				emit_content();
			}
			break;
		}
		return;
	}

	if (node->has_floating()) {
		const FloatingConfig &fc = node->get_floating();

		auto to_point = [](FloatingAttachPoint p) {
			switch (p) {
			case FloatingAttachPoint::LeftTop:
				return CLAY_ATTACH_POINT_LEFT_TOP;
			case FloatingAttachPoint::LeftCenter:
				return CLAY_ATTACH_POINT_LEFT_CENTER;
			case FloatingAttachPoint::LeftBottom:
				return CLAY_ATTACH_POINT_LEFT_BOTTOM;
			case FloatingAttachPoint::CenterTop:
				return CLAY_ATTACH_POINT_CENTER_TOP;
			case FloatingAttachPoint::Center:
				return CLAY_ATTACH_POINT_CENTER_CENTER;
			case FloatingAttachPoint::CenterBottom:
				return CLAY_ATTACH_POINT_CENTER_BOTTOM;
			case FloatingAttachPoint::RightTop:
				return CLAY_ATTACH_POINT_RIGHT_TOP;
			case FloatingAttachPoint::RightCenter:
				return CLAY_ATTACH_POINT_RIGHT_CENTER;
			case FloatingAttachPoint::RightBottom:
				return CLAY_ATTACH_POINT_RIGHT_BOTTOM;
			}
			return CLAY_ATTACH_POINT_LEFT_TOP;
		};

		const Clay_FloatingElementConfig float_cfg = {
			.offset = { .x = fc.offset.x, .y = fc.offset.y },
			.zIndex = fc.z_index,
			.attachPoints = { .element = to_point(fc.element_point), .parent = to_point(fc.parent_point) },
			.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
			.attachTo = fc.attach_to == FloatingAttachTo::Root ? CLAY_ATTACH_TO_ROOT : CLAY_ATTACH_TO_PARENT,
		};

		switch (cs.overflow) {
		case Overflow::Scroll:
			CLAY(clay_id,
				 { .layout = layout,
				   .aspectRatio = aspect_cfg,
				   .floating = float_cfg,
				   .clip = { .horizontal = false, .vertical = true, .childOffset = Clay_GetScrollOffset() } }) {
				emit_content();
			}
			break;
		case Overflow::Clip:
			CLAY(clay_id,
				 { .layout = layout,
				   .aspectRatio = aspect_cfg,
				   .floating = float_cfg,
				   .clip = { .horizontal = true, .vertical = true } }) {
				emit_content();
			}
			break;
		default:
			CLAY(clay_id, { .layout = layout, .aspectRatio = aspect_cfg, .floating = float_cfg }) {
				emit_content();
			}
			break;
		}
		return;
	}

	switch (cs.overflow) {
	case Overflow::Scroll:
		CLAY(clay_id,
			 {
				 .layout = layout,
				 .aspectRatio = aspect_cfg,
				 .clip = { .horizontal = false, .vertical = true, .childOffset = Clay_GetScrollOffset() },
			 }) {
			emit_content();
		}
		break;

	case Overflow::Clip:
		CLAY(clay_id,
			 {
				 .layout = layout,
				 .aspectRatio = aspect_cfg,
				 .clip = { .horizontal = true, .vertical = true },
			 }) {
			emit_content();
		}
		break;

	default:
		CLAY(clay_id, { .layout = layout, .aspectRatio = aspect_cfg }) {
			emit_content();
		}
		break;
	}
}

void LayoutEngine::update_rects(View *node, Vec2 parent_clay_pos, Vec2 accumulated_offset) {
	const ComputedStyle &cs = node->get_display_style();
	if (cs.display == Display::None) {
		node->set_layout_rect({});
		return;
	}

	const Clay_ElementId clay_id = make_element_id(node);
	const Clay_ElementData data = Clay_GetElementData(clay_id);

	Vec2 clay_pos = parent_clay_pos;
	Vec2 relative_offset = {};
	if (cs.position == Position::Relative) {
		const View *parent = node->get_parent();
		const Vec2 parent_size = parent != nullptr ? parent->get_layout_rect().size : Vec2(0.F);
		if (cs.left.unit != LengthUnit::Auto) {
			relative_offset.x = resolve_offset(cs.left, parent_size.x);
		} else if (cs.right.unit != LengthUnit::Auto) {
			relative_offset.x = -resolve_offset(cs.right, parent_size.x);
		}
		if (cs.top.unit != LengthUnit::Auto) {
			relative_offset.y = resolve_offset(cs.top, parent_size.y);
		} else if (cs.bottom.unit != LengthUnit::Auto) {
			relative_offset.y = -resolve_offset(cs.bottom, parent_size.y);
		}
	}
	const Vec2 subtree_offset = accumulated_offset + node->get_layout_anim_offset() + relative_offset;
	if (data.found) {
		const Clay_BoundingBox &bb = data.boundingBox;
		clay_pos = { bb.x, bb.y };
		node->set_layout_home(clay_pos);
		const Vec2 old_size = node->get_layout_rect().size;
		const Rect new_rect = { .position = clay_pos - parent_clay_pos, .size = { bb.width, bb.height } };
		if (new_rect != node->get_layout_rect()) {
			if (new_rect.size != old_size) {
				m_size_changed = true;
				m_resized_nodes.push_back(node);
				node->queue_redraw();
			}
			node->set_layout_rect(new_rect);
			node->mark_subtree_bounds_dirty();
		}
		const Vec2 abs_pos = clay_pos + subtree_offset;
		if (abs_pos != node->get_absolute_position()) {
			node->set_absolute_position(abs_pos);
			node->mark_subtree_bounds_dirty();
			node->queue_redraw();
		}
	}

	for (const auto &child : node->get_children()) {
		update_rects(child.get(), clay_pos, subtree_offset);
	}
}

} // namespace Aquila::UI::Core
