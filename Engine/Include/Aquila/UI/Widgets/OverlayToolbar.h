#ifndef AQUILA_UI_WIDGETS_OVERLAY_TOOLBAR_H
#define AQUILA_UI_WIDGETS_OVERLAY_TOOLBAR_H

#include "Aquila/UI/Core/View.h"

namespace Aquila::UI::Core {

enum class OverlayEdge : Uint8 { Left, Right, Top, Bottom };

/// A toolbar floating over its parent (the viewport). Dragging the grip docks it to the nearest edge:
/// vertical on the left and right, a row on the top and bottom. Markup sets edge="" and along="".
class OverlayToolbar : public View {
  public:
	OverlayToolbar();

	[[nodiscard]] std::string_view get_type_name() const override { return "OverlayToolbar"; }

	/// `along` is the offset along that edge, in pixels.
	void dock(OverlayEdge edge, F32 along);
	[[nodiscard]] OverlayEdge get_edge() const { return m_edge; }
	[[nodiscard]] F32 get_along() const { return m_along; }
	[[nodiscard]] bool is_horizontal() const { return m_edge == OverlayEdge::Top || m_edge == OverlayEdge::Bottom; }

	void set_margin(F32 margin);
	void set_z_index(Int16 z_index);

	void begin_drag(Vec2 cursor);
	void drag_to(Vec2 cursor);
	void end_drag();
	[[nodiscard]] bool is_dragging() const { return m_dragging; }

	using View::add_child;
	View *add_child(Unique<View> child) override;

	void apply_xml_attribute(std::string_view name, std::string_view value,
							 IResourceResolver *resolver = nullptr) override;

	Signal<void(OverlayEdge, F32)> on_docked;

	[[nodiscard]] static OverlayEdge nearest_edge(const Rect &area, const Rect &toolbar);

  private:
	void apply_orientation();
	void apply_floating();
	void orient_child(View *child) const;
	void update_dock_hint(OverlayEdge edge);
	void remove_dock_hint();
	[[nodiscard]] F32 clamp_along(OverlayEdge edge, F32 along, Vec2 size) const;

	View *m_grip = nullptr;
	View *m_hint = nullptr;
	OverlayEdge m_edge = OverlayEdge::Left;
	F32 m_along = 8.F;
	F32 m_margin = 6.F;
	Int16 m_z_index = 20;
	bool m_dragging = false;
	Vec2 m_grab_offset{};
};

}

#endif
