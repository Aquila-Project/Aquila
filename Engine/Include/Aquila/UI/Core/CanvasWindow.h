#ifndef AQUILA_UI_CORE_CANVAS_WINDOW_H
#define AQUILA_UI_CORE_CANVAS_WINDOW_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Platform/Cursor.h"
#include "Aquila/Rendering/IRenderWindowHost.h"

namespace Aquila::UI::Core {

class Canvas;
class LayoutLoader;

class CanvasWindow {
  public:
	CanvasWindow();
	virtual ~CanvasWindow();

	AQUILA_NONCOPYABLE(CanvasWindow);
	AQUILA_NONMOVEABLE(CanvasWindow);

	/// `on_closed` may destroy this.
	Aquila::Rendering::RenderWindowId open(Aquila::Rendering::IRenderWindowHost &host, const std::string &title,
										   Delegate<void()> on_closed, Vec2 min_size = Vec2(0.F));
	/// Fine to call from this window's own events.
	void close();

	void update(F32 delta_time);
	void render(Graphics::Renderer2D &batcher, GFX::GfxCommandList &cmd);
	void on_event(Platform::Events::Event &event);
	void set_frame_target(const void *target);
	[[nodiscard]] Platform::CursorType get_cursor() const;
	[[nodiscard]] Canvas *get_canvas() const { return m_canvas.get(); }

  protected:
	/// Call it first when building.
	Canvas &create_canvas(Uint32 width, Uint32 height, LayoutLoader &loader);

	/// Before the canvas updates.
	virtual void on_update(F32 delta_time) {}
	/// True keeps the event from the canvas.
	virtual bool intercept_event(Platform::Events::Event &event) { return false; }

  private:
	Unique<Canvas> m_canvas;
	Aquila::Rendering::IRenderWindowHost *m_host = nullptr;
	Aquila::Rendering::RenderWindowId m_window = nullptr;
};

}

#endif
