#include "Aquila/UI/Widgets/OverlayToolbar.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/UI/Widgets/Separator.h"

namespace Aquila::UI::Core {

namespace {

bool is_horizontal_edge(OverlayEdge edge) {
	return edge == OverlayEdge::Top || edge == OverlayEdge::Bottom;
}

Option<F32> parse_float(std::string_view text) {
	const std::string buffer(text);
	char *end = nullptr;
	const F32 value = std::strtof(buffer.c_str(), &end);
	if (end == buffer.c_str()) {
		return std::nullopt;
	}
	return value;
}

Option<OverlayEdge> parse_edge(std::string_view text) {
	if (text == "left") {
		return OverlayEdge::Left;
	}
	if (text == "right") {
		return OverlayEdge::Right;
	}
	if (text == "top") {
		return OverlayEdge::Top;
	}
	if (text == "bottom") {
		return OverlayEdge::Bottom;
	}
	return std::nullopt;
}

class OverlayToolbarGrip : public View {
  public:
	explicit OverlayToolbarGrip(OverlayToolbar *owner) : m_owner(owner) {
		set_input_leaf(true);
		add_class("overlay-toolbar-grip");
		set_tooltip("Drag to dock on another edge");
	}

	[[nodiscard]] std::string_view get_type_name() const override { return "OverlayToolbarGrip"; }

	void on_mouse_press(Platform::MouseButton btn, Vec2 pos) override {
		View::on_mouse_press(btn, pos);
		if (btn == Platform::MouseButton::Left) {
			m_owner->begin_drag(pos);
		}
	}

	void on_mouse_move(Vec2 pos) override {
		if (m_is_pressed && m_owner->is_dragging()) {
			m_owner->drag_to(pos);
		}
	}

	void on_mouse_release(Platform::MouseButton btn, Vec2 pos) override {
		View::on_mouse_release(btn, pos);
		if (btn == Platform::MouseButton::Left && m_owner->is_dragging()) {
			m_owner->end_drag();
		}
	}

	void on_draw_self(Rendering::DrawList &draw_list) override {
		View::on_draw_self(draw_list);
		const Rect rect = get_absolute_rect();
		const Vec4 color = get_display_style().color;
		const bool horizontal = m_owner->is_horizontal();
		const Int32 columns = horizontal ? 2 : 3;
		const Int32 rows = horizontal ? 3 : 2;
		const F32 radius = 1.F;
		const F32 pitch = 4.F;
		const Vec2 center = rect.position + (rect.size * 0.5F);

		for (Int32 col = 0; col < columns; ++col) {
			for (Int32 row = 0; row < rows; ++row) {
				const F32 x = center.x + ((static_cast<F32>(col) - (static_cast<F32>(columns - 1) * 0.5F)) * pitch);
				const F32 y = center.y + ((static_cast<F32>(row) - (static_cast<F32>(rows - 1) * 0.5F)) * pitch);
				const Rect dot = { .position = { x - radius, y - radius }, .size = { radius * 2.F, radius * 2.F } };
				draw_list.draw_rect(dot, color, Vec4(radius), 0.F, Vec4(0.F), 2);
			}
		}
	}

  private:
	OverlayToolbar *m_owner;
};

}

OverlayToolbar::OverlayToolbar() {
	add_class("overlay-toolbar");
	m_grip = View::add_child(std::make_unique<OverlayToolbarGrip>(this));
	apply_orientation();
	apply_floating();
}

View *OverlayToolbar::add_child(Unique<View> child) {
	View *raw = View::add_child(std::move(child));
	orient_child(raw);
	return raw;
}

void OverlayToolbar::dock(OverlayEdge edge, F32 along) {
	const bool orientation_changed = is_horizontal_edge(edge) != is_horizontal();
	m_edge = edge;
	m_along = along;
	if (orientation_changed) {
		apply_orientation();
	}
	apply_floating();
}

void OverlayToolbar::set_margin(F32 margin) {
	m_margin = margin;
	apply_floating();
}

void OverlayToolbar::set_z_index(Int16 z_index) {
	m_z_index = z_index;
	apply_floating();
}

OverlayEdge OverlayToolbar::nearest_edge(const Rect &area, const Rect &toolbar) {
	const Vec2 center = toolbar.position + (toolbar.size * 0.5F);
	const std::array<std::pair<F32, OverlayEdge>, 4> distances = { {
		{ center.x - area.position.x, OverlayEdge::Left },
		{ area.position.x + area.size.x - center.x, OverlayEdge::Right },
		{ center.y - area.position.y, OverlayEdge::Top },
		{ area.position.y + area.size.y - center.y, OverlayEdge::Bottom },
	} };

	auto best = distances.front();
	for (const auto &candidate : distances) {
		if (candidate.first < best.first) {
			best = candidate;
		}
	}
	return best.second;
}

void OverlayToolbar::begin_drag(Vec2 cursor) {
	if (get_parent() == nullptr || m_dragging) {
		return;
	}
	m_dragging = true;
	m_grab_offset = cursor - get_absolute_position();
	add_class("overlay-toolbar-dragging");
	drag_to(cursor);
}

void OverlayToolbar::drag_to(Vec2 cursor) {
	View *parent = get_parent();
	if (!m_dragging || parent == nullptr) {
		return;
	}

	const Rect area = parent->get_absolute_rect();
	const Vec2 size = get_layout_rect().size;
	Vec2 top_left = cursor - m_grab_offset - area.position;
	top_left.x = Math::clamp(top_left.x, 0.F, Math::max(0.F, area.size.x - size.x));
	top_left.y = Math::clamp(top_left.y, 0.F, Math::max(0.F, area.size.y - size.y));

	FloatingConfig floating;
	floating.attach_to = FloatingAttachTo::Parent;
	floating.element_point = FloatingAttachPoint::LeftTop;
	floating.parent_point = FloatingAttachPoint::LeftTop;
	floating.offset = top_left;
	floating.z_index = static_cast<Int16>(m_z_index + 10);
	set_floating(floating);
	invalidate_layout();

	update_dock_hint(nearest_edge(area, { .position = area.position + top_left, .size = size }));
}

void OverlayToolbar::end_drag() {
	if (!m_dragging) {
		return;
	}
	m_dragging = false;
	remove_class("overlay-toolbar-dragging");
	remove_dock_hint();

	View *parent = get_parent();
	if (parent == nullptr) {
		return;
	}

	const Rect area = parent->get_absolute_rect();
	const Rect current = get_absolute_rect();
	const OverlayEdge edge = nearest_edge(area, current);
	const bool flips = is_horizontal_edge(edge) != is_horizontal();
	const Vec2 docked_size = flips ? Vec2(current.size.y, current.size.x) : current.size;
	const F32 along =
		is_horizontal_edge(edge) ? current.position.x - area.position.x : current.position.y - area.position.y;

	dock(edge, clamp_along(edge, along, docked_size));
	on_docked(m_edge, m_along);
}

F32 OverlayToolbar::clamp_along(OverlayEdge edge, F32 along, Vec2 size) const {
	View *parent = get_parent();
	if (parent == nullptr) {
		return along;
	}
	const Vec2 area = parent->get_absolute_rect().size;
	const F32 room = is_horizontal_edge(edge) ? area.x - size.x : area.y - size.y;
	return Math::clamp(along, m_margin, Math::max(m_margin, room - m_margin));
}

void OverlayToolbar::apply_orientation() {
	const bool horizontal = is_horizontal();
	set_class("overlay-toolbar-horizontal", horizontal);
	set_class("overlay-toolbar-vertical", !horizontal);
	StyleProperties direction;
	direction.flex_direction = horizontal ? FlexDirection::Row : FlexDirection::Column;
	merge_style(direction);
	m_grip->set_class("overlay-toolbar-grip-horizontal", horizontal);
	m_grip->set_class("overlay-toolbar-grip-vertical", !horizontal);
	for (const auto &child : get_children()) {
		orient_child(child.get());
	}
	invalidate_layout();
}

void OverlayToolbar::orient_child(View *child) const {
	if (dynamic_cast<Separator *>(child) == nullptr) {
		return;
	}
	const bool horizontal = is_horizontal();
	child->set_class("overlay-toolbar-separator-vertical", horizontal);
	child->set_class("overlay-toolbar-separator-horizontal", !horizontal);
}

void OverlayToolbar::apply_floating() {
	FloatingConfig floating;
	floating.attach_to = FloatingAttachTo::Parent;
	floating.z_index = m_z_index;

	switch (m_edge) {
	case OverlayEdge::Left:
		floating.element_point = FloatingAttachPoint::LeftTop;
		floating.offset = { m_margin, m_along };
		break;
	case OverlayEdge::Right:
		floating.element_point = FloatingAttachPoint::RightTop;
		floating.offset = { -m_margin, m_along };
		break;
	case OverlayEdge::Top:
		floating.element_point = FloatingAttachPoint::LeftTop;
		floating.offset = { m_along, m_margin };
		break;
	case OverlayEdge::Bottom:
		floating.element_point = FloatingAttachPoint::LeftBottom;
		floating.offset = { m_along, -m_margin };
		break;
	}
	floating.parent_point = floating.element_point;

	set_floating(floating);
	invalidate_layout();
}

void OverlayToolbar::update_dock_hint(OverlayEdge edge) {
	View *parent = get_parent();
	if (parent == nullptr) {
		return;
	}
	if (m_hint == nullptr) {
		auto hint = std::make_unique<View>();
		hint->add_class("overlay-toolbar-dock-hint");
		hint->set_skip_hit_test(true);
		m_hint = parent->add_child(std::move(hint));
	}

	constexpr F32 k_thickness = 3.F;
	const Vec2 area = parent->get_absolute_rect().size;
	const bool horizontal = is_horizontal_edge(edge);

	StyleProperties size;
	size.width = StyleLength::pixel(horizontal ? area.x : k_thickness);
	size.height = StyleLength::pixel(horizontal ? k_thickness : area.y);
	m_hint->set_style(size);

	FloatingConfig floating;
	floating.attach_to = FloatingAttachTo::Parent;
	floating.z_index = static_cast<Int16>(m_z_index + 5);
	switch (edge) {
	case OverlayEdge::Left:
	case OverlayEdge::Top:
		floating.element_point = FloatingAttachPoint::LeftTop;
		break;
	case OverlayEdge::Right:
		floating.element_point = FloatingAttachPoint::RightTop;
		break;
	case OverlayEdge::Bottom:
		floating.element_point = FloatingAttachPoint::LeftBottom;
		break;
	}
	floating.parent_point = floating.element_point;
	m_hint->set_floating(floating);
	m_hint->invalidate_layout();
}

void OverlayToolbar::remove_dock_hint() {
	if (m_hint == nullptr) {
		return;
	}
	if (View *parent = get_parent()) {
		parent->remove_child(m_hint);
	}
	m_hint = nullptr;
}

void OverlayToolbar::apply_xml_attribute(std::string_view name, std::string_view value, IResourceResolver *resolver) {
	if (name == "edge") {
		if (const Option<OverlayEdge> edge = parse_edge(value)) {
			dock(*edge, m_along);
		} else {
			AQUILA_LOG_WARNING("OverlayToolbar: unknown edge '{}'", value);
		}
		return;
	}
	if (name == "along" || name == "margin" || name == "z") {
		const Option<F32> number = parse_float(value);
		if (!number) {
			AQUILA_LOG_WARNING("OverlayToolbar: '{}' expects a number, got '{}'", name, value);
			return;
		}
		if (name == "along") {
			dock(m_edge, *number);
		} else if (name == "margin") {
			set_margin(*number);
		} else {
			set_z_index(static_cast<Int16>(*number));
		}
		return;
	}
	View::apply_xml_attribute(name, value, resolver);
}

}
