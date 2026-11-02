#ifndef AQUILA_UI_WIDGETS_CLICKABLE_H
#define AQUILA_UI_WIDGETS_CLICKABLE_H

#include "Aquila/UI/Widgets/Control.h"

namespace Aquila::UI::Core {

/// Press and release over it, without a drag in between. Its children never get the press.
class Clickable : public Control {
  public:
	Clickable();

	[[nodiscard]] std::string_view get_type_name() const override { return "Clickable"; }

	Signal<void()> on_click;
	Signal<void(DragState &)> on_drag_start_requested;
	Signal<void(Platform::KeyCode, int)> on_key_pressed;

	void set_draggable(bool draggable) { m_is_draggable = draggable; }

	void on_mouse_press(Platform::MouseButton btn, Vec2 pos) override;
	void on_mouse_release(Platform::MouseButton btn, Vec2 pos) override;
	void on_drag_start(DragState &state) override;
	void on_key_press(Platform::KeyCode key, int mods = 0) override;

  private:
	bool m_click_armed = false;
};

}

#endif
