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
	/// Only with set_accepts_drop(true). Check the payload yourself, every drag over it fires them.
	Signal<void(DragState &)> on_drag_entered;
	Signal<void(DragState &)> on_drag_left;
	/// Fires before on_drag_left, inside the mouse release: don't rebuild the views under the cursor from here.
	Signal<void(DragState &)> on_dropped;
	Signal<void(Platform::KeyCode, int)> on_key_pressed;

	void set_draggable(bool draggable) { m_is_draggable = draggable; }
	void set_accepts_drop(bool accepts) { m_is_accepting_payload = accepts; }

	void on_mouse_press(Platform::MouseButton btn, Vec2 pos) override;
	void on_mouse_release(Platform::MouseButton btn, Vec2 pos) override;
	void on_drag_start(DragState &state) override;
	void on_drag_enter(DragState &state) override;
	void on_drag_leave(DragState &state) override;
	void on_drop(DragState &state) override;
	void on_key_press(Platform::KeyCode key, int mods = 0) override;

  private:
	bool m_click_armed = false;
};

}

#endif
