#ifndef AQUILA_WINDOW_H
#define AQUILA_WINDOW_H

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Math/MathTypes.h"

#include "Aquila/Platform/Platform.h"
#include "Aquila/Platform/Cursor.h"

#include "Aquila/Platform/Events/Event.h"
#include "Aquila/Platform/Events/WindowEvent.h"
#include "Aquila/Platform/Events/InputEvent.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Aquila::Application {

class Window {
  public:
	using EventCallbackFn = std::function<void(Platform::Events::Event &)>;

	Window(Uint32 width, Uint32 height, const std::string &title, bool maximized = true, bool start_hidden = false);
	~Window();

	void poll_events();
	/// Sleeps until an event comes in or the timeout runs out. No timeout waits forever.
	void wait_events(Option<F64> timeout_seconds = std::nullopt);
	/// Wakes wait_events() up, from any thread.
	static void wake();
	/// Of all connected monitors. Frames are paced to it.
	[[nodiscard]] static F64 highest_refresh_rate();
	bool should_close() const;
	[[nodiscard]] bool is_visible() const;
	/// Sends the merged mouse move. Call it after polling.
	void flush_pending_events();

	Uint32 get_width() const { return m_data.width; }
	Uint32 get_height() const { return m_data.height; }
	void set_title(const std::string &text) const;
	void set_cursor(Platform::CursorType type);
	/// For windows created with start_hidden.
	void show();
	void request_close() const;
	[[nodiscard]] Vec2 get_position() const;
	void set_position(Vec2 position) const;
	[[nodiscard]] std::string get_clipboard_text() const;
	void set_clipboard_text(const std::string &text) const;

	void set_event_callback(const EventCallbackFn &callback) { m_data.event_callback = callback; }

	// Called from inside the Win32 modal resize loop (WM_PAINT/refresh).
	// Hook this to render a frame during live resize so the window doesn't go black.
	void set_refresh_callback(std::function<void()> callback) { m_data.refresh_callback = std::move(callback); }

	bool is_window_resized() const { return m_data.resized; }
	void reset_resized_flag() { m_data.resized = false; }

	GLFWwindow *get_native_window() const { return m_window; }
	void create_window_surface(VkInstance instance, VkSurfaceKHR *surface) const;

  private:
	void initialize();
	void shutdown() const;
	void setup_callbacks();
	GLFWwindow *m_window;

	std::array<GLFWcursor *, static_cast<size_t>(Platform::CursorType::Count)> m_cursors{};
	Platform::CursorType m_current_cursor = Platform::CursorType::Arrow;

	struct WindowData {
		Window *owner = nullptr;
		std::string title;
		Uint32 width, height;
		bool resized = false;
		bool iconified = false;
		EventCallbackFn event_callback;
		std::function<void()> refresh_callback;
		F64 last_mouse_x = 0.F, last_mouse_y = 0.F;
		bool has_pending_mouse_move = false;
	};

	WindowData m_data;
	bool m_start_maximized = true;
	bool m_start_hidden = false;
	bool m_shown = false;
};

} // namespace Aquila::Application

#endif // AQUILA_WINDOW_H
