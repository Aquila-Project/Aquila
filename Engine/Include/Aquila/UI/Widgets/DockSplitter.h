#pragma once

#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/DockTypes.h"

namespace Aquila::UI::Core {

class DockSplitter : public View {
  public:
	explicit DockSplitter(SplitDirection dir);

	[[nodiscard]] std::string_view get_type_name() const override { return "DockSplitter"; }
	static constexpr ViewKind K_KIND = ViewKind::DockSplitter;
	[[nodiscard]] ViewKind get_kind() const override { return K_KIND; }

	void set_resize_before(bool v) { m_resize_before = v; }

	void on_mouse_press(Platform::MouseButton btn, Vec2 pos) override;
	void on_mouse_release(Platform::MouseButton btn, Vec2 pos) override;
	void on_mouse_move(Vec2 pos) override;
	void on_mouse_enter() override;
	void on_mouse_leave() override;

  private:
	void set_grabber_visible(bool visible);

	[[nodiscard]] View *resolve_after() const;
	[[nodiscard]] View *resolve_before() const;

	SplitDirection m_dir;
	View *m_grabber = nullptr;
	bool m_resize_before = true;
	Vec2 m_drag_start_pos{};
	float m_before_grow_start = 0.F;
	float m_after_grow_start = 0.F;
};

} // namespace Aquila::UI::Core
