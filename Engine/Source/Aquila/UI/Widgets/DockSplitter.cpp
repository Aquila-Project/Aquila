#include "Aquila/UI/Widgets/DockSplitter.h"

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/UI/Widgets/DockNode.h"

namespace Aquila::UI::Core {

DockSplitter::DockSplitter(SplitDirection dir) : m_dir(dir) {
	set_input_leaf(true);
	add_class("dock-splitter");
	add_class(dir == SplitDirection::Horizontal ? "dock-splitter-h" : "dock-splitter-v");

	FloatingConfig fc;
	fc.attach_to = FloatingAttachTo::Parent;
	fc.parent_point = FloatingAttachPoint::LeftTop;
	fc.element_point =
		(dir == SplitDirection::Horizontal) ? FloatingAttachPoint::CenterTop : FloatingAttachPoint::LeftCenter;
	fc.z_index = 30;
	set_floating(fc);

	auto grabber = std::make_unique<View>();
	grabber->add_class("dock-grabber");
	grabber->add_class(dir == SplitDirection::Horizontal ? "dock-grabber-h" : "dock-grabber-v");
	m_grabber = add_child(std::move(grabber));
	set_grabber_visible(false);
}

void DockSplitter::set_grabber_visible(bool visible) {
	if (m_grabber == nullptr) {
		return;
	}
	m_grabber->set_hidden(!visible);
}

void DockSplitter::on_mouse_enter() {
	View::on_mouse_enter();
	set_grabber_visible(true);
}

void DockSplitter::on_mouse_leave() {
	View::on_mouse_leave();
	if (!m_is_pressed) {
		set_grabber_visible(false);
	}
}

View *DockSplitter::resolve_after() const {
	return view_cast<DockNode>(get_parent());
}

View *DockSplitter::resolve_before() const {
	View *after = resolve_after();
	if (after == nullptr) {
		return nullptr;
	}
	View *container = after->get_parent();
	if (container == nullptr) {
		return nullptr;
	}
	View *prev = nullptr;
	for (const auto &child : container->get_children()) {
		if (child.get() == after) {
			return prev;
		}
		if (view_is<DockNode>(child.get())) {
			prev = child.get();
		}
	}
	return nullptr;
}

void DockSplitter::on_mouse_press(Platform::MouseButton btn, Vec2 pos) {
	View::on_mouse_press(btn, pos);
	View *before = resolve_before();
	View *after = resolve_after();
	if (btn != Platform::MouseButton::Left || before == nullptr || after == nullptr) {
		return;
	}
	m_drag_start_pos = pos;
	m_before_grow_start =
		(m_dir == SplitDirection::Horizontal) ? before->get_layout_rect().size.x : before->get_layout_rect().size.y;
	m_after_grow_start =
		(m_dir == SplitDirection::Horizontal) ? after->get_layout_rect().size.x : after->get_layout_rect().size.y;
}

void DockSplitter::on_mouse_release(Platform::MouseButton btn, Vec2 pos) {
	View::on_mouse_release(btn, pos);
	if (!is_hovered()) {
		set_grabber_visible(false);
	}
}

void DockSplitter::on_mouse_move(Vec2 pos) {
	View *before = resolve_before();
	View *after = resolve_after();
	View *container = (before != nullptr) ? before->get_parent() : nullptr;
	if (!m_is_pressed || before == nullptr || after == nullptr || container == nullptr) {
		return;
	}

	const float delta = (m_dir == SplitDirection::Horizontal) ? pos.x - m_drag_start_pos.x : pos.y - m_drag_start_pos.y;

	const float parent_px = (m_dir == SplitDirection::Horizontal) ? container->get_layout_rect().size.x
																  : container->get_layout_rect().size.y;
	if (parent_px <= 0.F) {
		return;
	}

	// Each side must stay at least kMin pixels wide/tall regardless of layout context.
	constexpr float K_MIN = 60.F;
	const float min_px = K_MIN;
	const float max_px = Math::max(min_px, parent_px - 5.F - K_MIN);

	StyleProperties sp;
	if (m_resize_before) {
		const float new_px = Math::clamp(m_before_grow_start + delta, min_px, max_px);
		const float new_pct = new_px / parent_px * 100.F;
		if (m_dir == SplitDirection::Horizontal) {
			sp.width = StyleLength::percent(new_pct);
		} else {
			sp.height = StyleLength::percent(new_pct);
		}
		before->merge_style(sp);
	} else {
		const float new_px = Math::clamp(m_after_grow_start - delta, min_px, max_px);
		const float new_pct = new_px / parent_px * 100.F;
		if (m_dir == SplitDirection::Horizontal) {
			sp.width = StyleLength::percent(new_pct);
		} else {
			sp.height = StyleLength::percent(new_pct);
		}
		after->merge_style(sp);
	}

	container->invalidate_layout();
}

} // namespace Aquila::UI::Core
