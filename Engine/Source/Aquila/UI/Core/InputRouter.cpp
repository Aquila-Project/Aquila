#include "Aquila/UI/Core/InputRouter.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/DrawCompositor.h"
#include "Aquila/Platform/Events/InputEvent.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Platform/Input.h"

namespace Aquila::UI::Core {

using namespace Platform::Events;

void InputRouter::set_focus(View *view) {
	if (m_focused_view == view) {
		return;
	}
	if (m_focused_view != nullptr) {
		m_focused_view->on_focus_lost();
	}
	m_focused_view = view;
	if (m_focused_view != nullptr) {
		m_focused_view->on_focus_gained();
	}
}

void InputRouter::on_view_removed(View *view) {
	if (m_drag_source_candidate == view) {
		m_drag_source_candidate = nullptr;
		m_drag_state.payload.reset();
		m_drag_state.label.clear();
		m_drag_state.is_dragging = false;
		m_canvas.hide_drag_ghost();
	}
}

void InputRouter::on_event(Platform::Events::Event &e) {
	EventDispatcher dispatcher(e);

	dispatcher.dispatch<MouseMovedEvent>([this](MouseMovedEvent &e) {
		m_mouse_pos = { e.get_x(), e.get_y() };

		if (m_drag_state.is_dragging) {
			m_canvas.move_drag_ghost(m_mouse_pos);
		}

		if (Platform::Input::is_mouse_button_pressed(MouseButton::Left) && !m_drag_state.is_dragging &&
			m_drag_source_candidate != nullptr) {
			Vec2 delta = m_mouse_pos - m_drag_start_pos;
			if (Math::length(delta) > 5.F) {
				m_drag_state.is_dragging = true;
				m_drag_source_candidate->on_drag_start(m_drag_state);
				m_drag_target = m_hovered_view ? m_hovered_view->get_first_parent_that_accepts_drop() : nullptr;
				if (m_drag_target) {
					m_drag_target->on_drag_enter(m_drag_state);
				}
				m_canvas.show_drag_ghost(m_drag_state.label, m_mouse_pos);
				m_canvas.mark_dirty();
			}
		}

		View *hit = m_compositor.hit_test(m_mouse_pos);
		if (hit != m_hovered_view) {
			if (m_hovered_view) {
				m_hovered_view->on_mouse_leave();
			}
			m_hovered_view = hit;
			if (m_hovered_view) {
				m_hovered_view->on_mouse_enter();
			}

			if (m_drag_state.is_dragging) {
				View *new_target = m_hovered_view ? m_hovered_view->get_first_parent_that_accepts_drop() : nullptr;
				if (new_target != m_drag_target) {
					if (m_drag_target) {
						m_drag_target->on_drag_leave(m_drag_state);
					}
					m_drag_target = new_target;
					if (m_drag_target) {
						m_drag_target->on_drag_enter(m_drag_state);
					}
				}
			}

			m_canvas.mark_dirty();
		}
		if (m_hovered_view) {
			m_hovered_view->on_mouse_hover(m_mouse_pos);
		}
		if (m_focused_view && m_focused_view->is_pressed()) {
			m_focused_view->on_mouse_move(m_mouse_pos);
		}
		return false;
	});

	dispatcher.dispatch<MouseButtonPressedEvent>([this](MouseButtonPressedEvent &e) {
		m_drag_source_candidate = nullptr;
		m_canvas.dismiss_popups_outside(m_hovered_view);
		if (!m_hovered_view) {
			set_focus(nullptr);
			return false;
		}
		set_focus(m_hovered_view);
		m_focused_view->on_mouse_press(e.get_mouse_button(), m_mouse_pos);

		m_drag_start_pos = Platform::Input::get_mouse_position();
		m_drag_source_candidate = m_hovered_view->get_first_draggable_parent();

		return true;
	});

	dispatcher.dispatch<MouseButtonReleasedEvent>([this](MouseButtonReleasedEvent &e) {
		if (m_drag_state.is_dragging) {
			if (m_hovered_view) {
				View *drop_candidate = m_hovered_view->get_first_parent_that_accepts_drop();

				if (drop_candidate) {
					drop_candidate->on_drop(m_drag_state);
				}
			}

			if (m_drag_target) {
				m_drag_target->on_drag_leave(m_drag_state);
				m_drag_target = nullptr;
			}
			m_canvas.hide_drag_ghost();
			m_drag_state.payload.reset();
			m_drag_state.label.clear();
			m_drag_state.is_dragging = false;
			m_drag_source_candidate = nullptr;
		}

		if (m_hovered_view) {
			m_hovered_view->on_mouse_release(e.get_mouse_button(), m_mouse_pos);
		}
		if (m_focused_view && m_focused_view != m_hovered_view) {
			m_focused_view->on_mouse_release(e.get_mouse_button(), m_mouse_pos);
		}
		return m_hovered_view != nullptr;
	});

	dispatcher.dispatch<MouseScrolledEvent>([this](MouseScrolledEvent &e) {
		const Vec2 delta(e.get_x_offset(), e.get_y_offset());
		for (View *view = m_hovered_view; view != nullptr; view = view->get_parent()) {
			if (view->on_scroll(delta)) {
				break;
			}
		}
		return m_hovered_view != nullptr && !m_hovered_view->get_pass_through_scroll();
	});

	dispatcher.dispatch<KeyPressedEvent>([this](KeyPressedEvent &e) {
		if (m_focused_view) {
			m_focused_view->on_key_press(e.get_key_code(), e.get_mods());
			return true; // consumed — prevents the engine layer from also acting on this key
		}
		return false;
	});

	dispatcher.dispatch<KeyReleasedEvent>([this](KeyReleasedEvent &e) {
		if (m_focused_view) {
			m_focused_view->on_key_release(e.get_key_code());
			return true;
		}
		return false;
	});

	dispatcher.dispatch<KeyTypedEvent>([this](KeyTypedEvent &e) {
		if (m_focused_view) {
			m_focused_view->on_char_input(static_cast<Uint32>(e.get_key_code()));
			return true;
		}
		return false;
	});
}

} // namespace Aquila::UI::Core
