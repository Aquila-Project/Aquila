#ifndef AQUILA_EVENT_H
#define AQUILA_EVENT_H
#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/Profiler.h"

namespace Aquila::Platform::Events {

/// The window that sent the event, opaque so Platform doesn't know about windows.
/// Application casts it back to `Window *`.
using EventSource = const void *;

enum class EventCategory : Uint8 {
	None = 0,
	Application = BIT(0),
	Input = BIT(1),
	Keyboard = BIT(2),
	Mouse = BIT(3),
	MouseButton = BIT(4),
	Scene = BIT(5),
	Renderer = BIT(6),
	AssetSelected = BIT(7)
};

inline EventCategory operator|(EventCategory a, EventCategory b) {
	return static_cast<EventCategory>(static_cast<int>(a) | static_cast<int>(b));
}
inline bool operator&(EventCategory a, EventCategory b) {
	return (static_cast<int>(a) & static_cast<int>(b)) != 0;
}

class Event {
  public:
	virtual ~Event() = default;
	[[nodiscard]] virtual const char *get_name() const = 0;
	[[nodiscard]] virtual EventCategory get_category() const = 0;
	[[nodiscard]] virtual std::type_index get_type_index() const = 0;
	[[nodiscard]] virtual std::string to_string() const { return get_name(); }
	[[nodiscard]] EventSource get_source() const { return m_source; }

	void set_source(EventSource source) { m_source = source; }
	bool handled = false;

  private:
	EventSource m_source = nullptr;
};

/// Runs the handler only when the event is of type T. The handler's return value becomes `handled`.
///
/// @code
/// EventDispatcher dispatcher(event);
/// dispatcher.dispatch<KeyPressedEvent>([&](KeyPressedEvent &key) { return on_key(key); });
/// @endcode
class EventDispatcher {
  public:
	explicit EventDispatcher(Event &event) : m_event(event) {}

	template <typename T, typename F> bool dispatch(const F &func) {
		if (m_event.get_type_index() == std::type_index(typeid(T))) {
			m_event.handled = func(static_cast<T &>(m_event));
			return true;
		}
		return false;
	}

  private:
	Event &m_event;
};

} // namespace Aquila::Platform::Events
#endif
