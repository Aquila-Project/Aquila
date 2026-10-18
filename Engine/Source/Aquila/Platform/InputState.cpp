#include "Aquila/Platform/InputState.h"

namespace Aquila::Platform {

using namespace Events;

void InputState::press(Button &button) {
	if (!button.down) {
		button.down = true;
		button.pressed = true;
	}
}

void InputState::release(Button &button) {
	if (button.down) {
		button.down = false;
		button.released = true;
	}
}

void InputState::on_event(Event &event) {
	EventDispatcher dispatcher(event);
	dispatcher.dispatch<KeyPressedEvent>([&](KeyPressedEvent &e) {
		if (Button *button = key(*this, e.get_key_code()); button != nullptr && !e.is_repeat()) {
			press(*button);
		}
		return false;
	});
	dispatcher.dispatch<KeyReleasedEvent>([&](KeyReleasedEvent &e) {
		if (Button *button = key(*this, e.get_key_code())) {
			release(*button);
		}
		return false;
	});
	dispatcher.dispatch<MouseButtonPressedEvent>([&](MouseButtonPressedEvent &e) {
		if (Button *button = mouse_button(*this, e.get_mouse_button())) {
			press(*button);
		}
		return false;
	});
	dispatcher.dispatch<MouseButtonReleasedEvent>([&](MouseButtonReleasedEvent &e) {
		if (Button *button = mouse_button(*this, e.get_mouse_button())) {
			release(*button);
		}
		return false;
	});
	dispatcher.dispatch<MouseMovedEvent>([this](MouseMovedEvent &e) {
		const Vec2 position(e.get_x(), e.get_y());
		if (m_has_mouse) {
			m_mouse_motion += position - m_mouse;
		}
		m_mouse = position;
		m_has_mouse = true;
		return false;
	});
	dispatcher.dispatch<MouseScrolledEvent>([this](MouseScrolledEvent &e) {
		m_scroll += Vec2(e.get_x_offset(), e.get_y_offset());
		return false;
	});
}

void InputState::end_frame() {
	for (Button &button : m_keys) {
		button.pressed = false;
		button.released = false;
	}
	for (Button &button : m_mouse_buttons) {
		button.pressed = false;
		button.released = false;
	}
	m_mouse_motion = Vec2(0.F);
	m_scroll = Vec2(0.F);
}

void InputState::release_all() {
	for (Button &button : m_keys) {
		release(button);
	}
	for (Button &button : m_mouse_buttons) {
		release(button);
	}
}

void InputState::clear() {
	m_keys = {};
	m_mouse_buttons = {};
	m_mouse_motion = Vec2(0.F);
	m_scroll = Vec2(0.F);
	m_has_mouse = false;
}

void InputState::set_view(Vec2 origin, Vec2 size) {
	m_view_origin = origin;
	m_view_size = size;
}

bool InputState::is_key_down(KeyCode code) const {
	const Button *button = key(*this, code);
	return button != nullptr && button->down;
}

bool InputState::is_key_just_pressed(KeyCode code) const {
	const Button *button = key(*this, code);
	return button != nullptr && button->pressed;
}

bool InputState::is_key_just_released(KeyCode code) const {
	const Button *button = key(*this, code);
	return button != nullptr && button->released;
}

bool InputState::is_mouse_button_down(MouseButton code) const {
	const Button *button = mouse_button(*this, code);
	return button != nullptr && button->down;
}

bool InputState::is_mouse_button_just_pressed(MouseButton code) const {
	const Button *button = mouse_button(*this, code);
	return button != nullptr && button->pressed;
}

bool InputState::is_mouse_button_just_released(MouseButton code) const {
	const Button *button = mouse_button(*this, code);
	return button != nullptr && button->released;
}

}
