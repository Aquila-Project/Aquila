#ifndef AQUILA_INPUT_H
#define AQUILA_INPUT_H

#include "Aquila/Platform/Events/InputEvent.h"

namespace Aquila::Platform {

using namespace Aquila::Platform::Events;

/// Polled input state, kept per window. Queries answer for the window that sent the last event.
class Input {
  public:
	Input() = delete;

	[[nodiscard]] static bool is_key_pressed(KeyCode key);
	[[nodiscard]] static bool is_mouse_button_pressed(MouseButton button);
	[[nodiscard]] static Vec2 get_mouse_position();

	/// Application feeds every event here.
	static void on_event(Event &event);
	/// Drops the state of a closed window.
	static void on_window_destroyed(EventSource window);
};

} // namespace Aquila::Platform
#endif
