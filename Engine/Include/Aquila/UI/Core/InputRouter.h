#pragma once

#include "Aquila/UI/Core/DragState.h"
#include "Aquila/UI/Core/View.h"

namespace Aquila::Platform::Events {
class Event;
}

namespace Aquila::UI::Core {

class Canvas;
class DrawCompositor;

class InputRouter {
  public:
	InputRouter(Canvas &canvas, DrawCompositor &compositor) : m_canvas(canvas), m_compositor(compositor) {}

	void on_event(Platform::Events::Event &event);
	void set_focus(View *view); // handles OnFocusLost/OnFocusGained bookkeeping
	void on_view_removed(View *view);

	[[nodiscard]] Vec2 mouse_pos() const { return m_mouse_pos; }
	[[nodiscard]] View *hovered_view() const { return m_hovered_view; }
	[[nodiscard]] View *focused_view() const { return m_focused_view; }

  private:
	Canvas &m_canvas;
	DrawCompositor &m_compositor;

	ViewRef m_hovered_view;
	ViewRef m_focused_view;
	ViewRef m_drag_source_candidate;
	ViewRef m_drag_target;
	DragState m_drag_state{};

	Vec2 m_mouse_pos = {};
	Vec2 m_drag_start_pos = {};
};

} // namespace Aquila::UI::Core
