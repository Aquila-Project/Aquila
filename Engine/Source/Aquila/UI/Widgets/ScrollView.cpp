#include "Aquila/UI/Widgets/ScrollView.h"

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/UI/Style/StyleProperties.h"

namespace Aquila::UI::Core {

namespace {

class ScrollThumb : public View {
  public:
	explicit ScrollThumb(ScrollView *owner) : m_owner(owner) {}

	[[nodiscard]] std::string_view get_type_name() const override { return "ScrollThumb"; }

	void on_mouse_press(Platform::MouseButton btn, Vec2 pos) override {
		View::on_mouse_press(btn, pos);
		if (btn == Platform::MouseButton::Left) {
			m_dragging = true;
			m_owner->begin_thumb_drag(pos.y);
		}
	}

	void on_mouse_release(Platform::MouseButton btn, Vec2 pos) override {
		View::on_mouse_release(btn, pos);
		if (btn == Platform::MouseButton::Left) {
			m_dragging = false;
		}
	}

	void on_mouse_move(Vec2 pos) override {
		View::on_mouse_move(pos);
		if (m_dragging) {
			m_owner->drag_thumb(pos.y);
		}
	}

  private:
	ScrollView *m_owner;
	bool m_dragging = false;
};

constexpr float K_MIN_THUMB_HEIGHT = 24.F;

} // namespace

ScrollView::ScrollView() {
	add_class("scroll-view");

	auto inner = std::make_unique<View>();
	inner->add_class("scroll-inner");
	m_inner = View::add_child(std::move(inner));

	auto track = std::make_unique<View>();
	track->add_class("scroll-bar-track");
	FloatingConfig cfg;
	cfg.attach_to = FloatingAttachTo::Parent;
	cfg.element_point = FloatingAttachPoint::RightTop;
	cfg.parent_point = FloatingAttachPoint::RightTop;
	cfg.z_index = 20;
	track->set_floating(cfg);
	track->set_hidden(true);
	m_track = View::add_child(std::move(track));

	auto thumb = std::make_unique<ScrollThumb>(this);
	thumb->add_class("scroll-bar-thumb");
	m_thumb = m_track->add_child(std::move(thumb));
}

View *ScrollView::add_child(Unique<View> child) {
	return m_inner->View::add_child(std::move(child));
}

void ScrollView::remove_oldest_content() {
	const auto &children = m_inner->get_children();
	if (!children.empty()) {
		m_inner->remove_child(children.front().get());
	}
}

void ScrollView::scroll_to_bottom() {
	constexpr float K_END_OF_CONTENT = 1.0e9F;
	set_scroll_offset(K_END_OF_CONTENT);
}

bool ScrollView::is_at_bottom() const {
	const ScrollMetrics m = measure();
	return m.max_offset <= 0.F || Math::min(get_scroll_target(), m.max_offset) >= m.max_offset - 1.F;
}

void ScrollView::on_scroll_layout() {
	update_scrollbar();
}

bool ScrollView::on_scroll(Vec2 delta) {
	const ScrollMetrics m = measure();
	if (m.max_offset <= 0.F) {
		return false;
	}

	constexpr float K_WHEEL_SPEED = 40.F;
	const float from = Math::clamp(get_scroll_target(), 0.F, m.max_offset);
	set_scroll_offset(Math::clamp(from - delta.y * K_WHEEL_SPEED, 0.F, m.max_offset));
	return true;
}

ScrollView::ScrollMetrics ScrollView::measure() const {
	ScrollMetrics m{};
	m.viewport_h = get_layout_rect().size.y;
	m.content_h = get_scroll_content_height();
	m.max_offset = Math::max(m.content_h - m.viewport_h, 0.F);
	m.offset = get_scroll_offset();

	const float ratio = (m.content_h > 0.F) ? (m.viewport_h / m.content_h) : 1.F;
	m.thumb_h = Math::min(Math::max(m.viewport_h * ratio, K_MIN_THUMB_HEIGHT), m.viewport_h);
	m.thumb_travel = m.viewport_h - m.thumb_h;
	return m;
}

void ScrollView::update_scrollbar() {
	const ScrollMetrics m = measure();
	const bool visible = m.max_offset > 0.5F && m.viewport_h > 0.F;
	if (visible != m_last_visible) {
		m_track->set_hidden(!visible);
		m_last_visible = visible;
	}
	if (!visible) {
		return;
	}

	if (m.thumb_h != m_last_thumb_h) {
		StyleProperties sp;
		sp.height = StyleLength::pixel(m.thumb_h);
		m_thumb->merge_style(sp);
		m_last_thumb_h = m.thumb_h;
	}

	const float thumb_y = (m.max_offset > 0.F) ? (m.offset / m.max_offset) * m.thumb_travel : 0.F;
	m_thumb->set_layout_anim_offset({ 0.F, thumb_y });
}

void ScrollView::begin_thumb_drag(float mouse_y) {
	m_drag_start_mouse_y = mouse_y;
	m_drag_start_offset = get_scroll_offset();
}

void ScrollView::drag_thumb(float mouse_y) {
	const ScrollMetrics m = measure();
	if (m.thumb_travel <= 0.F || m.max_offset <= 0.F) {
		return;
	}

	const float delta = mouse_y - m_drag_start_mouse_y;
	set_scroll_offset(Math::clamp(m_drag_start_offset + delta * (m.max_offset / m.thumb_travel), 0.F, m.max_offset));
}

} // namespace Aquila::UI::Core
