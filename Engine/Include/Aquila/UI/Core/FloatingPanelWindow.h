#ifndef AQUILA_UI_CORE_FLOATING_PANEL_WINDOW_H
#define AQUILA_UI_CORE_FLOATING_PANEL_WINDOW_H

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
namespace Aquila::UI::Core {
class Canvas;
class View;
class DockSpace;

class FloatingPanelWindow {
  public:
	FloatingPanelWindow();
	~FloatingPanelWindow();

	void build(Unique<View> panel_subtree, const std::string &title, Uint32 width, Uint32 height,
			   const std::vector<std::string> &style_paths);

	void update(F32 delta_time);
	void render(Graphics::Renderer2D &batcher, GFX::GfxCommandList &cmd);
	void on_event(Platform::Events::Event &event);

	[[nodiscard]] bool has_content() const;
	Unique<View> detach_content();
	[[nodiscard]] const std::string &get_title() const { return m_title; }
	[[nodiscard]] DockSpace *get_dock_space() const { return m_dock_space; }
	[[nodiscard]] Canvas *get_canvas() const { return m_canvas.get(); }
	[[nodiscard]] Platform::CursorType get_cursor() const;

  private:
	Unique<Canvas> m_canvas;
	View *m_body = nullptr;
	DockSpace *m_dock_space = nullptr;
	std::string m_title;
};

}

#endif
