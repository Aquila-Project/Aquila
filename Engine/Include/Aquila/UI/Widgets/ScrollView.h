#pragma once

#include "Aquila/UI/Core/View.h"

namespace Aquila::UI::Core {

class ScrollView : public View {
  public:
	ScrollView();

	[[nodiscard]] std::string_view get_type_name() const override { return "ScrollView"; }

	using View::add_child;
	/// Goes into the scrolled content, not the view itself.
	View *add_child(Unique<View> child) override;

	/// Drops the first child, for log style lists that grow at the bottom.
	void remove_oldest_content();
	void scroll_to_bottom();
	[[nodiscard]] bool is_at_bottom() const;

	void on_style_resolved() override;
	bool on_update(F32 delta_time) override;
	bool on_scroll(Vec2 delta) override;

	void begin_thumb_drag(float mouse_y);
	void drag_thumb(float mouse_y);

  private:
	struct ScrollMetrics {
		float viewport_h;
		float content_h;
		float offset;
		float max_offset;
		float thumb_h;
		float thumb_travel;
	};

	ScrollMetrics measure() const;
	void update_scrollbar();

	View *m_inner = nullptr;
	View *m_track = nullptr;
	View *m_thumb = nullptr;

	bool m_tick_registered = false;
	float m_drag_start_mouse_y = 0.F;
	float m_drag_start_offset = 0.F;

	bool m_last_visible = true;
	float m_last_track_h = -1.F;
	float m_last_thumb_h = -1.F;
	float m_last_thumb_y = -1.F;
};

} // namespace Aquila::UI::Core
