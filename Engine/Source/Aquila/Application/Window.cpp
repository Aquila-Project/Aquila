#include "Aquila/Application/Window.h"
#include "Aquila/Platform/Events/InputEvent.h"
#include "Aquila/Platform/Events/WindowEvent.h"
#include "GLFW/glfw3.h"

namespace Aquila::Application {

static bool s_GLFWInitialized = false;

static void glfw_error_callback(int error, const char *description) {
	AQUILA_LOG_ERROR("GLFW Error ({}): {}", error, description);
}

Window::Window(const Uint32 width, const Uint32 height, const std::string &title, bool maximized, bool start_hidden)
	: m_start_maximized(maximized), m_start_hidden(start_hidden) {
	m_data.title = title;
	m_data.width = width;
	m_data.height = height;
	m_data.owner = this;

	initialize();
}

Window::~Window() {
	shutdown();
}

void Window::initialize() {
	if (!s_GLFWInitialized) {
		glfwSetErrorCallback(glfw_error_callback);

		#ifdef AQUILA_PLATFORM_LINUX
		glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
		#endif

		const int success = glfwInit();
		AQUILA_ASSERT(success, "Could not initialize GLFW!");
		s_GLFWInitialized = true;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	glfwWindowHint(GLFW_VISIBLE, m_start_hidden ? GLFW_FALSE : GLFW_TRUE);

	m_window = glfwCreateWindow(m_data.width, m_data.height, m_data.title.c_str(), nullptr, nullptr);
	m_shown = !m_start_hidden;

	glfwSetWindowUserPointer(m_window, &m_data);
	if (m_start_maximized) {
		glfwMaximizeWindow(m_window);
	}
	setup_callbacks();

	// Sync dimensions after maximize — the WM_SIZE fires before SetupCallbacks so
	// the size callback never ran for it; query the actual size directly.
	int actual_w = 0;
	int actual_h = 0;
	glfwGetWindowSize(m_window, &actual_w, &actual_h);
	m_data.width = static_cast<Uint32>(actual_w);
	m_data.height = static_cast<Uint32>(actual_h);

	AQUILA_LOG_INFO("Window created: {}x{}", m_data.width, m_data.height);
}

void Window::set_title(const std::string &text) const {
	const auto title = std::string(m_data.title + " | " + text);
	glfwSetWindowTitle(m_window, title.c_str());
}
void Window::setup_callbacks() {
	glfwSetWindowSizeCallback(m_window, [](GLFWwindow *window, int width, int height) {
		WindowData &data = *static_cast<WindowData *>(glfwGetWindowUserPointer(window));
		data.width = width;
		data.height = height;
		data.resized = true;

		Platform::Events::WindowResizeEvent event(width, height);
		event.set_source(data.owner);
		data.event_callback(event);
	});

	glfwSetWindowCloseCallback(m_window, [](GLFWwindow *window) {
		const WindowData &data = *static_cast<WindowData *>(glfwGetWindowUserPointer(window));
		Platform::Events::WindowCloseEvent event(static_cast<bool>(glfwWindowShouldClose(window)));
		event.set_source(data.owner);
		data.event_callback(event);
	});

	glfwSetKeyCallback(m_window, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
		const WindowData &data = *static_cast<WindowData *>(glfwGetWindowUserPointer(window));
		switch (action) {
		case GLFW_PRESS: {
			Platform::Events::KeyPressedEvent event(static_cast<Platform::Events::KeyCode>(key), 0, mods);
			event.set_source(data.owner);

			data.event_callback(event);
			break;
		}
		case GLFW_RELEASE: {
			Platform::Events::KeyReleasedEvent event(static_cast<Platform::Events::KeyCode>(key));
			event.set_source(data.owner);

			data.event_callback(event);
			break;
		}
		case GLFW_REPEAT: {
			Platform::Events::KeyPressedEvent event(static_cast<Platform::Events::KeyCode>(key), 1, mods);
			event.set_source(data.owner);

			data.event_callback(event);
			break;
		}
		default:;
		}
	});

	glfwSetCharCallback(m_window, [](GLFWwindow *window, unsigned int keycode) {
		const WindowData &data = *static_cast<WindowData *>(glfwGetWindowUserPointer(window));
		Platform::Events::KeyTypedEvent event(static_cast<Platform::Events::KeyCode>(keycode));
		event.set_source(data.owner);

		data.event_callback(event);
	});

	glfwSetMouseButtonCallback(m_window, [](GLFWwindow *window, int button, int action, int mods) {
		const WindowData &data = *static_cast<WindowData *>(glfwGetWindowUserPointer(window));

		switch (action) {
		case GLFW_PRESS: {
			Platform::Events::MouseButtonPressedEvent event(static_cast<Platform::Events::MouseButton>(button));
			event.set_source(data.owner);

			data.event_callback(event);
			break;
		}
		case GLFW_RELEASE: {
			Platform::Events::MouseButtonReleasedEvent event(static_cast<Platform::Events::MouseButton>(button));
			event.set_source(data.owner);

			data.event_callback(event);
			break;
		}
		default:;
		}
	});

	glfwSetScrollCallback(m_window, [](GLFWwindow *window, const F64 x_offset, const F64 y_offset) {
		const WindowData &data = *static_cast<WindowData *>(glfwGetWindowUserPointer(window));
		Platform::Events::MouseScrolledEvent event(static_cast<F32>(x_offset), static_cast<F32>(y_offset));
		event.set_source(data.owner);

		data.event_callback(event);
	});

	// Store the latest position; PollEvents fires one synthesized event after draining the queue.
	glfwSetCursorPosCallback(m_window, [](GLFWwindow *window, const F64 x_pos, const F64 y_pos) {
		WindowData &data = *static_cast<WindowData *>(glfwGetWindowUserPointer(window));
		data.last_mouse_x = x_pos;
		data.last_mouse_y = y_pos;
		data.has_pending_mouse_move = true;
	});

	glfwSetWindowIconifyCallback(m_window, [](GLFWwindow *window, const int iconified) {
		WindowData &data = *static_cast<WindowData *>(glfwGetWindowUserPointer(window));
		data.iconified = iconified == GLFW_TRUE;
		if (!data.iconified && data.refresh_callback) {
			data.refresh_callback();
		}
	});

	glfwSetWindowFocusCallback(m_window, [](GLFWwindow *window, const int focused) {
		const WindowData &data = *static_cast<WindowData *>(glfwGetWindowUserPointer(window));
		Platform::Events::WindowFocusEvent event(focused == GLFW_TRUE);
		event.set_source(data.owner);

		data.event_callback(event);
	});

	// On Windows, glfwPollEvents() is blocked inside Win32's modal resize loop while
	// the user drags a window border.  The refresh callback fires from inside that loop
	// whenever the window needs repainting (every mouse move during the drag), giving
	// us a chance to render a live frame.
	glfwSetWindowRefreshCallback(m_window, [](GLFWwindow *window) {
		WindowData &data = *static_cast<WindowData *>(glfwGetWindowUserPointer(window));
		if (data.refresh_callback) {
			data.refresh_callback();
		}
	});
}

void Window::flush_pending_events() {
	if (m_data.has_pending_mouse_move) {
		Platform::Events::MouseMovedEvent event(m_data.last_mouse_x, m_data.last_mouse_y);
		event.set_source(m_data.owner);

		m_data.event_callback(event);
		m_data.has_pending_mouse_move = false;
	}
}

void Window::poll_events() {
	glfwPollEvents();
}

void Window::wait_events(Option<F64> timeout_seconds) {
	if (!timeout_seconds) {
		glfwWaitEvents();
	} else if (*timeout_seconds > 0.0) {
		glfwWaitEventsTimeout(*timeout_seconds);
	} else {
		glfwPollEvents();
	}
}

void Window::wake() {
	glfwPostEmptyEvent();
}

F64 Window::highest_refresh_rate() {
	int count = 0;
	GLFWmonitor **monitors = glfwGetMonitors(&count);
	int highest = 0;
	for (int i = 0; i < count; ++i) {
		if (const GLFWvidmode *mode = glfwGetVideoMode(monitors[i])) {
			highest = std::max(highest, mode->refreshRate);
		}
	}
	return highest > 0 ? static_cast<F64>(highest) : 60.0;
}

bool Window::is_visible() const {
	return m_shown && !m_data.iconified;
}

bool Window::should_close() const {
	return glfwWindowShouldClose(m_window) != 0;
}

static int to_glfw_cursor(Platform::CursorType type) {
	switch (type) {
	case Platform::CursorType::Text:
		return GLFW_IBEAM_CURSOR;
	case Platform::CursorType::Hand:
		return GLFW_POINTING_HAND_CURSOR;
	case Platform::CursorType::Crosshair:
		return GLFW_CROSSHAIR_CURSOR;
	case Platform::CursorType::ResizeHorizontal:
		return GLFW_RESIZE_EW_CURSOR;
	case Platform::CursorType::ResizeVertical:
		return GLFW_RESIZE_NS_CURSOR;
	case Platform::CursorType::ResizeDiagonalTLBR:
		return GLFW_RESIZE_NWSE_CURSOR;
	case Platform::CursorType::ResizeDiagonalBLTR:
		return GLFW_RESIZE_NESW_CURSOR;
	case Platform::CursorType::ResizeAll:
		return GLFW_RESIZE_ALL_CURSOR;
	case Platform::CursorType::NotAllowed:
		return GLFW_NOT_ALLOWED_CURSOR;
	default:
		return GLFW_ARROW_CURSOR;
	}
}

std::string Window::get_clipboard_text() const {
	const char *text = glfwGetClipboardString(m_window);
	return text != nullptr ? text : "";
}

void Window::set_clipboard_text(const std::string &text) const {
	glfwSetClipboardString(m_window, text.c_str());
}

void Window::show() {
	glfwShowWindow(m_window);
	m_shown = true;
}

void Window::request_close() const {
	glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}

Vec2 Window::get_position() const {
	int x = 0;
	int y = 0;
	glfwGetWindowPos(m_window, &x, &y);
	return { static_cast<F32>(x), static_cast<F32>(y) };
}

void Window::set_position(Vec2 position) const {
	glfwSetWindowPos(m_window, static_cast<int>(position.x), static_cast<int>(position.y));
}

void Window::set_min_size(Vec2 size) const {
	glfwSetWindowSizeLimits(m_window, static_cast<int>(size.x), static_cast<int>(size.y), GLFW_DONT_CARE,
							GLFW_DONT_CARE);
}

void Window::set_cursor(Platform::CursorType type) {
	if (type == m_current_cursor) {
		return;
	}
	m_current_cursor = type;

	const size_t index = static_cast<size_t>(type);
	if (m_cursors[index] == nullptr) {
		m_cursors[index] = glfwCreateStandardCursor(to_glfw_cursor(type));
	}
	glfwSetCursor(m_window, m_cursors[index]);
}

void Window::shutdown() const {
	for (GLFWcursor *cursor : m_cursors) {
		if (cursor != nullptr) {
			glfwDestroyCursor(cursor);
		}
	}
	glfwDestroyWindow(m_window);
}

} // namespace Aquila::Application
