#ifndef AQUILA_RENDERING_I_RENDER_WINDOW_HOST_H
#define AQUILA_RENDERING_I_RENDER_WINDOW_HOST_H

#include "Aquila/Foundation/Math/MathTypes.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Platform/Cursor.h"

namespace Aquila::Graphics {
class Renderer2D;
}

namespace Aquila::GFX {
class GfxCommandList;
}

namespace Aquila::Platform::Events {
class Event;
}

namespace Aquila::Rendering {

/// Opaque, only compare it and pass it back.
using RenderWindowId = const void *;

struct RenderWindowCallbacks {
	Delegate<void(F32)> on_update;
	Delegate<void(Graphics::Renderer2D &, GFX::GfxCommandList &)> on_render;
	Delegate<void(Platform::Events::Event &)> on_event;
	Delegate<void()> on_close;
	Delegate<Platform::CursorType()> cursor;
	/// Also send this window's events to the modules, like floating dock panels need.
	bool forward_events = false;
};

/// Opens extra OS windows that render through the shared Renderer2D. Application implements it.
class IRenderWindowHost {
  public:
	virtual ~IRenderWindowHost() = default;

	virtual RenderWindowId create_window(Uint32 width, Uint32 height, const std::string &title,
										 RenderWindowCallbacks callbacks) = 0;
	virtual void request_close(RenderWindowId window) = 0;

	[[nodiscard]] virtual RenderWindowId get_main_window() const = 0;
	[[nodiscard]] virtual Vec2 get_window_position(RenderWindowId window) const = 0;
	[[nodiscard]] virtual Vec2 get_window_size(RenderWindowId window) const = 0;
	virtual void set_window_position(RenderWindowId window, Vec2 position) = 0;
};

/// Opens a window around `content`, which needs update(dt), render(r2d, cmd) and on_event(event).
/// get_cursor() and set_frame_target(window) are used when it has them.
/// `content` must outlive the window.
///
/// @code
/// m_window = open_content_window(host, *m_settings, 640, 480, "Settings", [this] { m_settings_open = false; });
/// @endcode
template <typename Content>
RenderWindowId open_content_window(IRenderWindowHost &host, Content &content, Uint32 width, Uint32 height,
								   const std::string &title, Delegate<void()> on_close) {
	RenderWindowCallbacks callbacks;
	callbacks.on_update = [&content](F32 dt) { content.update(dt); };
	callbacks.on_render = [&content](Graphics::Renderer2D &batcher, GFX::GfxCommandList &cmd) {
		content.render(batcher, cmd);
	};
	callbacks.on_event = [&content](Platform::Events::Event &event) { content.on_event(event); };
	callbacks.on_close = std::move(on_close);
	const RenderWindowId window = host.create_window(width, height, title, std::move(callbacks));
	if constexpr (requires { content.set_frame_target(window); }) {
		content.set_frame_target(window);
	}
	return window;
}

}

#endif
