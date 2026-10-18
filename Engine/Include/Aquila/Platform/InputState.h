#ifndef AQUILA_PLATFORM_INPUT_STATE_H
#define AQUILA_PLATFORM_INPUT_STATE_H

#include "Aquila/Foundation/Math/MathTypes.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Platform/Events/InputEvent.h"

namespace Aquila::Platform {

/// Input as one consumer sees it, the game for instance: held keys and buttons, what changed since the last
/// end_frame(), mouse motion and wheel. Unlike Input it's only fed the events you pass it.
///
/// @code
/// state.on_event(event);
/// if (state.is_key_just_pressed(KeyCode::Space)) { jump(); }
/// state.end_frame();
/// @endcode
class InputState {
  public:
	void on_event(Events::Event &event);
	/// Forgets the just pressed/released keys and buttons, the mouse motion and the wheel. Call it once the frame
	/// read them.
	void end_frame();
	/// Lets go of everything held, they read as just released until the next end_frame(). For focus loss.
	void release_all();
	/// Back to nothing held and nothing pending, the view stays.
	void clear();

	/// Mouse positions are relative to `origin` (window pixels) and the view is `size` big. Default: the whole
	/// window, size zero.
	void set_view(Vec2 origin, Vec2 size);
	[[nodiscard]] Vec2 get_view_size() const { return m_view_size; }

	[[nodiscard]] bool is_key_down(Events::KeyCode key) const;
	[[nodiscard]] bool is_key_just_pressed(Events::KeyCode key) const;
	[[nodiscard]] bool is_key_just_released(Events::KeyCode key) const;
	[[nodiscard]] bool is_mouse_button_down(Events::MouseButton button) const;
	[[nodiscard]] bool is_mouse_button_just_pressed(Events::MouseButton button) const;
	[[nodiscard]] bool is_mouse_button_just_released(Events::MouseButton button) const;

	/// Pixels, relative to the view's top left corner. Can be outside the view.
	[[nodiscard]] Vec2 get_mouse_position() const { return m_mouse - m_view_origin; }
	/// Pixels the mouse moved since the last end_frame().
	[[nodiscard]] Vec2 get_mouse_motion() const { return m_mouse_motion; }
	/// Wheel steps since the last end_frame(), y is up/down.
	[[nodiscard]] Vec2 get_scroll() const { return m_scroll; }

  private:
	struct Button {
		bool down = false;
		bool pressed = false;
		bool released = false;
	};

	static void press(Button &button);
	static void release(Button &button);
	/// Null for codes past the tables. Self is InputState or const InputState.
	template <typename Self> static auto *key(Self &self, Events::KeyCode code) {
		const auto index = static_cast<Usize>(static_cast<Uint16>(code));
		return index < self.m_keys.size() ? &self.m_keys[index] : nullptr;
	}
	template <typename Self> static auto *mouse_button(Self &self, Events::MouseButton code) {
		const auto index = static_cast<Usize>(static_cast<Uint8>(code));
		return index < self.m_mouse_buttons.size() ? &self.m_mouse_buttons[index] : nullptr;
	}

	std::array<Button, SharedConstants::MAX_KEY_STATES> m_keys{};
	std::array<Button, SharedConstants::MAX_MOUSE_STATES> m_mouse_buttons{};
	Vec2 m_mouse{ 0.F };
	Vec2 m_mouse_motion{ 0.F };
	Vec2 m_scroll{ 0.F };
	bool m_has_mouse = false;
	Vec2 m_view_origin{ 0.F };
	Vec2 m_view_size{ 0.F };
};

}

#endif
