#ifndef AQUILA_INPUT_EVENTS_H
#define AQUILA_INPUT_EVENTS_H

#include "Aquila/Platform/Events/Event.h"

namespace Aquila::Platform::Events {

/// Same values as GLFW keys.
enum class KeyCode : Uint16 {
	Unknown = 0,
	Space = 32,
	Minus = 45,
	Num0 = 48,
	Equal = 61,
	A = 65,
	B = 66,
	C = 67,
	D = 68,
	E = 69,
	F = 70,
	G = 71,
	H = 72,
	I = 73,
	J = 74,
	K = 75,
	L = 76,
	M = 77,
	N = 78,
	O = 79,
	P = 80,
	Q = 81,
	R = 82,
	S = 83,
	T = 84,
	U = 85,
	V = 86,
	W = 87,
	X = 88,
	Y = 89,
	Z = 90,
	Escape = 256,
	Enter = 257,
	Tab = 258,
	Backspace = 259,
	Delete = 261,
	Right = 262,
	Left = 263,
	Down = 264,
	Up = 265,
	Home = 268,
	End = 269,
	F1 = 290,
	F2 = 291,
	F3 = 292,
	F4 = 293,
	F5 = 294,
	F6 = 295,
	F7 = 296,
	F8 = 297,
	F9 = 298,
	F10 = 299,
	F11 = 300,
	F12 = 301,
	LeftShift = 340,
	LeftControl = 341,
	LeftAlt = 342,
	RightShift = 344,
	RightControl = 345,
	RightAlt = 346,
	KeypadSubtract = 333,
	KeypadAdd = 334,
	KeypadEnter = 335,
};

enum class MouseButton : Uint8 { Left = 0, Right = 1, Middle = 2 };

static constexpr int MODIFIER_SHIFT = 0x0001;
static constexpr int MODIFIER_CONTROL = 0x0002;
static constexpr int MODIFIER_ALT = 0x0004;

class KeyPressedEvent final : public Event {
  public:
	KeyPressedEvent(const KeyCode keycode, const int repeat_count, const int mods = 0)
		: m_key_code(keycode), m_repeat_count(repeat_count), m_mods(mods) {}

	[[nodiscard]] KeyCode get_key_code() const { return m_key_code; }
	[[nodiscard]] int get_repeat_count() const { return m_repeat_count; }
	[[nodiscard]] bool is_repeat() const { return m_repeat_count > 0; }
	[[nodiscard]] int get_mods() const { return m_mods; }
	[[nodiscard]] bool is_ctrl() const { return (m_mods & MODIFIER_CONTROL) != 0; }
	[[nodiscard]] bool is_shift() const { return (m_mods & MODIFIER_SHIFT) != 0; }
	[[nodiscard]] bool is_alt() const { return (m_mods & MODIFIER_ALT) != 0; }

	[[nodiscard]] std::string to_string() const override {
		return std::string("KeyPressedEvent: ") + std::to_string(static_cast<int>(m_key_code)) + " (" +
			std::to_string(m_repeat_count) + " repeats)";
	}

	EVENT_CLASS_TYPE(KeyPressedEvent)
	EVENT_CLASS_CATEGORY(EventCategory::Keyboard | EventCategory::Input)

  private:
	KeyCode m_key_code;
	int m_repeat_count;
	int m_mods;
};

class KeyReleasedEvent final : public Event {
  public:
	explicit KeyReleasedEvent(const KeyCode keycode) : m_key_code(keycode) {}

	[[nodiscard]] KeyCode get_key_code() const { return m_key_code; }

	[[nodiscard]] std::string to_string() const override {
		return std::string("KeyReleasedEvent: ") + std::to_string(static_cast<int>(m_key_code));
	}

	EVENT_CLASS_TYPE(KeyReleasedEvent)
	EVENT_CLASS_CATEGORY(EventCategory::Keyboard | EventCategory::Input)

  private:
	KeyCode m_key_code;
};

/// Text input. get_key_code() is really the typed Unicode codepoint, not a key.
class KeyTypedEvent final : public Event {
  public:
	explicit KeyTypedEvent(const KeyCode keycode) : m_key_code(keycode) {}

	[[nodiscard]] KeyCode get_key_code() const { return m_key_code; }

	EVENT_CLASS_TYPE(KeyTypedEvent)
	EVENT_CLASS_CATEGORY(EventCategory::Keyboard | EventCategory::Input)

  private:
	KeyCode m_key_code;
};

/// One per event poll with the latest position, the moves in between are merged.
/// Window pixels, top-left origin.
class MouseMovedEvent final : public Event {
  public:
	MouseMovedEvent(const F32 x, const F32 y) : m_mouse_x(x), m_mouse_y(y) {}

	[[nodiscard]] F32 get_x() const { return m_mouse_x; }
	[[nodiscard]] F32 get_y() const { return m_mouse_y; }

	[[nodiscard]] std::string to_string() const override {
		return "MouseMovedEvent: " + std::to_string(m_mouse_x) + ", " + std::to_string(m_mouse_y);
	}

	EVENT_CLASS_TYPE(MouseMovedEvent)
	EVENT_CLASS_CATEGORY(EventCategory::Mouse | EventCategory::Input)

  private:
	F32 m_mouse_x, m_mouse_y;
};

class MouseScrolledEvent final : public Event {
  public:
	MouseScrolledEvent(const F32 x_offset, const F32 y_offset) : m_x_offset(x_offset), m_y_offset(y_offset) {}

	[[nodiscard]] F32 get_x_offset() const { return m_x_offset; }
	[[nodiscard]] F32 get_y_offset() const { return m_y_offset; }

	[[nodiscard]] std::string to_string() const override {
		return "MouseScrolledEvent: " + std::to_string(m_x_offset) + ", " + std::to_string(m_y_offset);
	}

	EVENT_CLASS_TYPE(MouseScrolledEvent)
	EVENT_CLASS_CATEGORY(EventCategory::Mouse | EventCategory::Input)

  private:
	F32 m_x_offset, m_y_offset;
};

class MouseButtonPressedEvent final : public Event {
  public:
	explicit MouseButtonPressedEvent(const MouseButton button) : m_button(button) {}

	[[nodiscard]] MouseButton get_mouse_button() const { return m_button; }

	[[nodiscard]] std::string to_string() const override {
		return "MouseButtonPressedEvent: " + std::to_string(static_cast<int>(m_button));
	}

	EVENT_CLASS_TYPE(MouseButtonPressedEvent)
	EVENT_CLASS_CATEGORY(EventCategory::Mouse | EventCategory::Input | EventCategory::MouseButton)

  private:
	MouseButton m_button;
};

class MouseButtonReleasedEvent final : public Event {
  public:
	explicit MouseButtonReleasedEvent(const MouseButton button) : m_button(button) {}

	[[nodiscard]] MouseButton get_mouse_button() const { return m_button; }

	[[nodiscard]] std::string to_string() const override {
		return "MouseButtonReleasedEvent: " + std::to_string(static_cast<int>(m_button));
	}

	EVENT_CLASS_TYPE(MouseButtonReleasedEvent)
	EVENT_CLASS_CATEGORY(EventCategory::Mouse | EventCategory::Input | EventCategory::MouseButton)

  private:
	MouseButton m_button;
};

} // namespace Aquila::Platform::Events

#endif // AQUILA_INPUT_EVENTS_H
