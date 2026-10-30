#include "Aquila/UI/Widgets/Clickable.h"

namespace Aquila::UI::Core {

Clickable::Clickable() {
	set_input_leaf(true);
}

void Clickable::on_mouse_press(Platform::MouseButton btn, Vec2 pos) {
	if (btn == Platform::MouseButton::Left) {
		m_click_armed = true;
	}
	View::on_mouse_press(btn, pos);
}

void Clickable::on_mouse_release(Platform::MouseButton btn, Vec2 pos) {
	const bool clicked = btn == Platform::MouseButton::Left && m_click_armed && m_is_hovered;
	if (btn == Platform::MouseButton::Left) {
		m_click_armed = false;
	}
	View::on_mouse_release(btn, pos);
	if (clicked) {
		on_click();
	}
}

void Clickable::on_drag_start(DragState &state) {
	m_click_armed = false;
	View::on_drag_start(state);
	on_drag_start_requested(state);
}

void Clickable::on_key_press(Platform::KeyCode key, int mods) {
	on_key_pressed(key, mods);
}
}
