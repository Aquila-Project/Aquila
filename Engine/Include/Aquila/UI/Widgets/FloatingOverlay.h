#pragma once

#include "Aquila/UI/Core/View.h"

namespace Aquila::UI::Core {

class Button;

/// Base of popups, menus and tooltips: drawn above everything, closed by a click outside
/// unless set_dismiss_on_click_away(false).
class FloatingOverlay : public View {
  public:
	void open();
	void close();
	void toggle();
	[[nodiscard]] bool is_open() const { return m_open; }

	void set_dismiss_on_click_away(bool v);

	View *hit_test_absolute(Vec2 canvas_pos) override;

  protected:
	explicit FloatingOverlay(int16_t backdrop_z_tier = 49);

	void apply_display_state();

	bool m_open = false;
	bool m_dismiss_on_click_away = true;
};

} // namespace Aquila::UI::Core
