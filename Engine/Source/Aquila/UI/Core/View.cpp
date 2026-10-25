#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/ComponentInstance.h"
#include "Aquila/UI/Core/FontRegistry.h"
#include "Aquila/UI/Rendering/DrawCmd.h"
#include "Aquila/UI/Style/StyleParserHelper.h"
#include "Aquila/UI/Style/StylePropertyList.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Math/Math.h"

#include <charconv>

namespace Aquila::UI::Core {

static Uint32 s_NextStableId = 0;

namespace {

Option<UI::FloatingAttachPoint> parse_attach_point(std::string_view value) {
	using UI::FloatingAttachPoint;
	if (value == "left-top") {
		return FloatingAttachPoint::LeftTop;
	}
	if (value == "left-center") {
		return FloatingAttachPoint::LeftCenter;
	}
	if (value == "left-bottom") {
		return FloatingAttachPoint::LeftBottom;
	}
	if (value == "center-top") {
		return FloatingAttachPoint::CenterTop;
	}
	if (value == "center") {
		return FloatingAttachPoint::Center;
	}
	if (value == "center-bottom") {
		return FloatingAttachPoint::CenterBottom;
	}
	if (value == "right-top") {
		return FloatingAttachPoint::RightTop;
	}
	if (value == "right-center") {
		return FloatingAttachPoint::RightCenter;
	}
	if (value == "right-bottom") {
		return FloatingAttachPoint::RightBottom;
	}
	return std::nullopt;
}

Option<F32> parse_number(std::string_view text) {
	while (!text.empty() && (text.front() == ' ' || text.front() == ',')) {
		text.remove_prefix(1);
	}
	F32 number = 0.F;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
	if (error != std::errc{}) {
		return std::nullopt;
	}
	return number;
}

Option<Vec2> parse_vec2(std::string_view text) {
	const Usize split = text.find_first_of(" ,");
	if (split == std::string_view::npos) {
		return std::nullopt;
	}
	const Option<F32> x = parse_number(text.substr(0, split));
	const Option<F32> y = parse_number(text.substr(split));
	if (!x || !y) {
		return std::nullopt;
	}
	return Vec2{ *x, *y };
}

} // namespace

View::View() : m_stable_id(++s_NextStableId) {}

View::~View() {
	m_component_instance.reset();
	m_alive.reset();
	std::vector<Unique<View>> children = std::exchange(m_children, {});
	children.clear();
}

void View::set_component_instance(Unique<ComponentInstance> instance) {
	m_component_instance = std::move(instance);
}

Unique<ComponentInstance> View::take_component_instance() {
	return std::move(m_component_instance);
}

static F32 apply_easing(F32 t, UI::TransitionEasing easing) {
	switch (easing) {
	case UI::TransitionEasing::EaseIn:
		return t * t;
	case UI::TransitionEasing::EaseOut:
		return 1.F - ((1.F - t) * (1.F - t));
	case UI::TransitionEasing::EaseInOut:
		return t < 0.5F ? 2.F * t * t : 1.F - (((-2.F * t) + 2.F) * (-2.F * t + 2.F) * 0.5F);
	case UI::TransitionEasing::Ease:
		return t * t * (3.F - (2.F * t));
	case UI::TransitionEasing::Linear:
	default:
		return t;
	}
}

void View::set_computed_style(UI::ComputedStyle style) {
	if (!m_display_style_initialized) {
		m_display_style = style;
		m_animation_from = style;
		m_display_style_initialized = true;
	} else if (m_computed_style != style) {
		m_animation_from = m_display_style;
		m_transition_timer = 0.F;
		m_is_animation_finished = false;
		if (m_canvas != nullptr) {
			m_canvas->notify_animation_started(this);
		}
	}
	m_computed_style = std::move(style);
}

void View::update_animation(F32 delta_time) {
	const F32 duration_sec = m_computed_style.transition_duration / 1000.F;

	if (m_is_animation_finished) {
		return;
	}

	if (duration_sec <= 0.0001F) {
		m_display_style = m_computed_style;
		m_transition_timer = 0.F;
		m_is_animation_finished = true;
		return;
	}

	m_transition_timer = Math::min(m_transition_timer + delta_time, duration_sec);
	const F32 raw = m_transition_timer / duration_sec;
	const F32 alpha = apply_easing(raw, m_computed_style.transition_easing);

	m_display_style = m_computed_style;

#define AQ_STYLE_PROP(css, sp, cs, layout, anim, inherit) \
	AQ_STYLE_WHEN(anim, m_display_style.cs = Math::lerp(m_animation_from.cs, m_computed_style.cs, alpha);)
	AQ_STYLE_PROPERTY_LIST
#undef AQ_STYLE_PROP

	if (m_transition_timer >= duration_sec) {
		m_is_animation_finished = true;
	}
}

void View::apply_xml_attribute(std::string_view name, std::string_view value, IResourceResolver * /*resolver*/) {
	if (name == "tooltip") {
		set_tooltip(std::string(value));
		return;
	}
	if (name.starts_with("float-")) {
		apply_floating_attribute(name.substr(6), value);
		return;
	}
	StyleProperties props;
	UI::ParserHelper::apply_declaration(props, name, value);
	merge_style(props);
}

void View::apply_floating_attribute(std::string_view name, std::string_view value) {
	if (!m_floating) {
		m_floating = FloatingConfig{};
	}

	if (name == "attach") {
		if (value == "root") {
			m_floating->attach_to = UI::FloatingAttachTo::Root;
		} else if (value == "parent") {
			m_floating->attach_to = UI::FloatingAttachTo::Parent;
		} else {
			AQUILA_LOG_WARNING("View: unknown float-attach '{}'", value);
		}
	} else if (name == "element" || name == "parent") {
		const Option<UI::FloatingAttachPoint> point = parse_attach_point(value);
		if (!point) {
			AQUILA_LOG_WARNING("View: unknown float-{} '{}'", name, value);
			return;
		}
		(name == "element" ? m_floating->element_point : m_floating->parent_point) = *point;
	} else if (name == "offset") {
		if (const Option<Vec2> offset = parse_vec2(value)) {
			m_floating->offset = *offset;
		} else {
			AQUILA_LOG_WARNING("View: float-offset expects 'x y', got '{}'", value);
		}
	} else if (name == "z") {
		if (const Option<F32> z = parse_number(value)) {
			m_floating->z_index = static_cast<Int16>(*z);
		} else {
			AQUILA_LOG_WARNING("View: float-z expects a number, got '{}'", value);
		}
	} else {
		AQUILA_LOG_WARNING("View: unknown attribute 'float-{}'", name);
	}
}

void View::on_style_resolved() {
	const std::string &family = get_computed_style().font_family;
	m_resolved_font = family.empty() ? nullptr : FontRegistry::resolve(family);
}

const Rect &View::get_subtree_bounds() {
	if (!m_subtree_bounds_dirty) {
		return m_subtree_bounds;
	}
	m_subtree_bounds = get_absolute_rect();
	for (const auto &child : m_children) {
		if (child->get_display_style().display != Display::None && child->is_visible()) {
			m_subtree_bounds = m_subtree_bounds.Union(child->get_subtree_bounds());
		}
	}
	m_subtree_bounds_dirty = false;
	return m_subtree_bounds;
}

void View::set_enabled(bool enabled) {
	if (m_enabled == enabled) {
		return;
	}
	m_enabled = enabled;
	mark_style_dirty();
}

void View::request_focus() {
	if (m_canvas != nullptr) {
		m_canvas->notify_focus_request(this);
	}
}

void View::merge_style(const StyleProperties &overlay) {
	StyleProperties &style = m_style;
#define AQ_STYLE_PROP(css, sp, cs, layout, anim, inherit) \
	if (overlay.sp) {                                     \
		style.sp = overlay.sp;                            \
	}
	AQ_STYLE_PROPERTY_LIST
#undef AQ_STYLE_PROP

	if (overlay.min) {
		style.min = overlay.min;
	}
	if (overlay.max) {
		style.max = overlay.max;
	}
	if (overlay.padding_left) {
		style.padding_left = overlay.padding_left;
	}
	if (overlay.padding_right) {
		style.padding_right = overlay.padding_right;
	}
	if (overlay.padding_top) {
		style.padding_top = overlay.padding_top;
	}
	if (overlay.padding_bottom) {
		style.padding_bottom = overlay.padding_bottom;
	}

	mark_style_dirty();
}

void View::add_class(std::string cls) {
	if (std::ranges::find(m_classes, cls) != m_classes.end()) {
		return;
	}
	m_classes.push_back(std::move(cls));
	mark_style_dirty();
}

void View::remove_class(std::string_view cls) {
	auto it = std::ranges::find(m_classes, cls);
	if (it != m_classes.end()) {
		m_classes.erase(it);
		mark_style_dirty();
	}
}

void View::set_class(std::string cls, bool enabled) {
	if (enabled) {
		add_class(std::move(cls));
	} else {
		remove_class(cls);
	}
}

void View::set_hidden(bool hidden) {
	set_class("hidden", hidden);
}

View *View::add_child(Unique<View> child) {
	View *raw = child.get();
	raw->m_parent = this;
	raw->set_canvas(m_canvas);
	m_children.push_back(std::move(child));
	mark_subtree_bounds_dirty();
	raw->set_draw_dirty();
	if (m_canvas != nullptr) {
		m_canvas->notify_style_dirty(raw);
		m_canvas->notify_draw_dirty(raw);
	}
	return raw;
}

void View::queue_redraw() {
	if (m_canvas != nullptr) {
		m_canvas->notify_draw_dirty(this);
	}
}

void View::set_scroll_offset(F32 offset) {
	if (offset == m_scroll_target) {
		return;
	}
	m_scroll_target = offset;
	invalidate_layout();
}

void View::invalidate_layout() {
	if (m_canvas != nullptr) {
		m_canvas->notify_layout_dirty(this);
	}
}

void View::mark_style_dirty() {
	if (m_canvas != nullptr) {
		m_canvas->notify_style_dirty(this);
	}
}

void ViewRef::assign(View *view) {
	m_ptr = view;
	m_alive = view != nullptr ? view->alive_token() : WeakRef<void>{};
}

void View::set_canvas(Canvas *canvas) {
	m_canvas = canvas;
	if (canvas != nullptr) {
		canvas->notify_style_dirty(this);
		canvas->notify_draw_dirty(this);
	}
	for (const auto &child : m_children) {
		child->set_canvas(canvas);
	}
}

void View::notify_removed(View *node) {
	for (const auto &child : node->m_children) {
		notify_removed(child.get());
	}
	if (node->m_canvas != nullptr) {
		node->m_canvas->notify_view_removed(node);
	}
	node->m_canvas = nullptr;
}

void View::remove_child(View *child) {
	auto iterator = std::ranges::find_if(m_children, [child](const Unique<View> &view) { return view.get() == child; });
	if (iterator != m_children.end()) {
		notify_removed(child);
		m_children.erase(iterator);
		mark_subtree_bounds_dirty();
		invalidate_layout();
	}
}

void View::clear_children() {
	while (!m_children.empty()) {
		remove_child(m_children.back().get());
	}
}

Unique<View> View::detach_child(View *child) {
	auto it = std::ranges::find_if(m_children, [child](const Unique<View> &view) { return view.get() == child; });
	if (it == m_children.end()) {
		return nullptr;
	}
	notify_removed(child);
	child->m_parent = nullptr;
	auto owned = std::move(*it);
	m_children.erase(it);
	mark_subtree_bounds_dirty();
	invalidate_layout();
	return owned;
}

View *View::replace_child(View *old, Unique<View> new_child) {
	auto it = std::ranges::find_if(m_children, [old](const Unique<View> &view) { return view.get() == old; });
	if (it == m_children.end()) {
		return nullptr;
	}
	notify_removed(old);
	*it = std::move(new_child);
	View *raw = it->get();
	raw->m_parent = this;
	raw->set_canvas(m_canvas);
	raw->set_draw_dirty();
	if (m_canvas != nullptr) {
		m_canvas->notify_style_dirty(raw);
		m_canvas->notify_draw_dirty(raw);
	}
	return raw;
}

void View::reorder_child(View *child, View *before) {
	if (child == before) {
		return;
	}
	auto it = std::ranges::find_if(m_children, [child](const Unique<View> &view) { return view.get() == child; });
	if (it == m_children.end()) {
		return;
	}

	auto next = it + 1;
	const bool already_positioned =
		(before == nullptr) ? (next == m_children.end()) : (next != m_children.end() && next->get() == before);
	if (already_positioned) {
		return;
	}

	Unique<View> owned = std::move(*it);
	m_children.erase(it);

	if (before == nullptr) {
		m_children.push_back(std::move(owned));
	} else {
		auto dest =
			std::ranges::find_if(m_children, [before](const Unique<View> &view) { return view.get() == before; });
		m_children.insert(dest, std::move(owned));
	}

	mark_subtree_bounds_dirty();
	child->set_draw_dirty();
	invalidate_layout();
}

View *View::get_parent() const {
	return m_parent;
}

View *View::get_first_draggable_parent() const {
	auto cursor = const_cast<View *>(this);
	while (cursor != nullptr && !cursor->is_draggable()) {
		cursor = cursor->get_parent();
	}

	return cursor;
}

View *View::get_first_parent_that_accepts_drop() const {
	auto cursor = const_cast<View *>(this);
	while (cursor != nullptr && !cursor->is_accepting_payload()) {
		cursor = cursor->get_parent();
	}

	return cursor;
}

const std::vector<Unique<View>> &View::get_children() const {
	return m_children;
}

View *View::find_by_id(std::string_view id) {
	if (m_id == id) {
		return this;
	}

	for (const auto &child : m_children) {
		if (View *found = child->find_by_id(id)) {
			return found;
		}
	}

	return nullptr;
}

void View::on_mouse_enter() {
	if (m_is_hovered) {
		return;
	}
	m_is_hovered = true;
	mark_style_dirty();
	on_mouse_entered();
}

void View::on_mouse_leave() {
	if (!m_is_hovered) {
		return;
	}
	m_is_hovered = false;
	mark_style_dirty();
	on_mouse_left();
}

void View::on_mouse_press(Platform::MouseButton btn, Vec2 pos) {
	if (btn == Platform::MouseButton::Left) {
		m_is_pressed = true;
		mark_style_dirty();
		on_pressed(pos);
	}
	if (btn == Platform::MouseButton::Right) {
		on_context_menu(pos);
	}
}

void View::on_mouse_release(Platform::MouseButton btn, Vec2) {
	if (btn == Platform::MouseButton::Left) {
		m_is_pressed = false;
		mark_style_dirty();
	}
}

void View::on_focus_gained() {
	m_is_focused = true;
	mark_style_dirty();
}

void View::on_focus_lost() {
	m_is_focused = false;
	mark_style_dirty();
}

void View::on_drag_start(DragState &state) {}
void View::on_drop(DragState &state) {}
void View::on_drag_enter(DragState &state) {}
void View::on_drag_leave(DragState &state) {}

View *View::hit_test_absolute(Vec2 canvas_pos) {
	if (m_computed_style.display == Display::None || !m_visible) {
		return nullptr;
	}

	if (m_is_input_leaf) {
		const Rect r = get_absolute_rect();
		return r.contains(canvas_pos) ? this : nullptr;
	}

	for (int i = static_cast<int>(m_children.size()) - 1; i >= 0; --i) {
		if (View *hit = m_children[i]->hit_test_absolute(canvas_pos)) {
			return hit;
		}
	}

	const Rect r = get_absolute_rect();
	return r.contains(canvas_pos) ? this : nullptr;
}

void View::on_draw_self(Rendering::DrawList &draw_list) {
	if (!m_visible || m_computed_style.display == Display::None) {
		return;
	}

	const Rect world_rect = { .position = m_absolute_position, .size = m_layout_rect.size };

	for (const auto &shadow : m_display_style.box_shadows) {
		draw_list.draw_shadow(world_rect, shadow.offset, shadow.blur, shadow.spread, shadow.color,
							  m_display_style.border_radius, 0);
	}

	if (m_display_style.background_color.a > 0.F || m_display_style.border_width > 0.F) {
		draw_list.draw_rect(world_rect, m_display_style.background_color, m_display_style.border_radius,
							m_display_style.border_width, m_display_style.border_color, 1,
							m_display_style.border_style);
	}
}

} // namespace Aquila::UI::Core
