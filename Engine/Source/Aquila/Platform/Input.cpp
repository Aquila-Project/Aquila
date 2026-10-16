#include "Aquila/Platform/Input.h"
#include "Aquila/Foundation/SharedConstants.h"

namespace Aquila::Platform {

namespace {

struct InputState {
	std::array<bool, SharedConstants::MAX_KEY_STATES> key_states{};
	std::array<bool, SharedConstants::MAX_MOUSE_STATES> mouse_button_states{};
	F32 mouse_x = 0.F;
	F32 mouse_y = 0.F;
};

std::unordered_map<EventSource, InputState> s_states;
EventSource s_active_window = nullptr;

Usize key_index(KeyCode key) {
	return static_cast<Usize>(static_cast<Uint16>(key));
}

Usize button_index(MouseButton button) {
	return static_cast<Usize>(static_cast<Uint8>(button));
}

InputState *active_state() {
	if (s_active_window == nullptr) {
		return nullptr;
	}
	return &s_states[s_active_window];
}

}

bool Input::is_key_pressed(KeyCode key) {
	const InputState *state = active_state();
	const Usize index = key_index(key);
	return state != nullptr && index < state->key_states.size() && state->key_states[index];
}

bool Input::is_mouse_button_pressed(MouseButton button) {
	const InputState *state = active_state();
	const Usize index = button_index(button);
	return state != nullptr && index < state->mouse_button_states.size() && state->mouse_button_states[index];
}

Vec2 Input::get_mouse_position() {
	const InputState *state = active_state();
	return state != nullptr ? Vec2(state->mouse_x, state->mouse_y) : Vec2(0.F);
}

void Input::on_event(Event &event) {
	EventSource window = event.get_source();
	if (window == nullptr) {
		return;
	}
	InputState &state = s_states[window];
	s_active_window = window;

	auto set_key = [&state](KeyCode key, bool pressed) {
		if (const Usize index = key_index(key); index < state.key_states.size()) {
			state.key_states[index] = pressed;
		}
	};
	auto set_button = [&state](MouseButton button, bool pressed) {
		if (const Usize index = button_index(button); index < state.mouse_button_states.size()) {
			state.mouse_button_states[index] = pressed;
		}
	};

	EventDispatcher dispatcher(event);
	dispatcher.dispatch<KeyPressedEvent>([&](KeyPressedEvent &e) {
		set_key(e.get_key_code(), true);
		return false;
	});
	dispatcher.dispatch<KeyReleasedEvent>([&](KeyReleasedEvent &e) {
		set_key(e.get_key_code(), false);
		return false;
	});
	dispatcher.dispatch<MouseButtonPressedEvent>([&](MouseButtonPressedEvent &e) {
		set_button(e.get_mouse_button(), true);
		return false;
	});
	dispatcher.dispatch<MouseButtonReleasedEvent>([&](MouseButtonReleasedEvent &e) {
		set_button(e.get_mouse_button(), false);
		return false;
	});
	dispatcher.dispatch<MouseMovedEvent>([&](MouseMovedEvent &e) {
		state.mouse_x = e.get_x();
		state.mouse_y = e.get_y();
		return false;
	});
}

void Input::on_window_destroyed(EventSource window) {
	s_states.erase(window);
	if (s_active_window == window) {
		s_active_window = nullptr;
	}
}

}
