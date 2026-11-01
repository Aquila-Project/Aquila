#include "UI/Viewport/ViewportCanvas.h"

namespace Editor {

using namespace Aquila;

namespace {

class CanvasLayer final : public UI::Core::View {
  public:
	explicit CanvasLayer(const ViewportCanvas &owner) : m_owner(owner) {
		set_skip_hit_test(true);
		add_class("viewport-canvas-layer");
	}

	[[nodiscard]] std::string_view get_type_name() const override { return "ViewportCanvasLayer"; }

	void on_draw_self(UI::Rendering::DrawList &draw_list) override { m_owner.draw(draw_list); }

  private:
	const ViewportCanvas &m_owner;
};

}

ViewportCanvas::ViewportCanvas(Int16 z_index) {
	set_pass_through_scroll(true);
	add_class("viewport-canvas");

	UI::FloatingConfig floating;
	floating.attach_to = UI::FloatingAttachTo::Parent;
	floating.element_point = UI::FloatingAttachPoint::LeftTop;
	floating.parent_point = UI::FloatingAttachPoint::LeftTop;
	floating.z_index = z_index;
	set_floating(floating);

	m_layer = add_child<CanvasLayer>(*this);
}

void ViewportCanvas::fit(const Rect &viewport) {
	const Vec2 offset =
		viewport.position - (get_parent() != nullptr ? get_parent()->get_absolute_position() : Vec2(0.F));
	if (get_layout_rect().size == viewport.size && get_floating().offset == offset) {
		return;
	}
	UI::StyleProperties size;
	size.width = UI::StyleLength::pixel(viewport.size.x);
	size.height = UI::StyleLength::pixel(viewport.size.y);
	merge_style(size);
	UI::FloatingConfig floating = get_floating();
	floating.offset = offset;
	set_floating(floating);
	invalidate_layout();
}

void ViewportCanvas::redraw() {
	m_layer->queue_redraw();
}

UI::Text::FontAtlas *ViewportCanvas::font() const {
	return m_layer->get_resolved_font();
}

F32 ViewportCanvas::font_size() const {
	const F32 size = m_layer->get_display_style().font_size;
	return size > 0.F ? size : 12.F;
}

}
