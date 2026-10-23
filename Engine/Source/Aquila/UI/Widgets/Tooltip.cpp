#include "Aquila/UI/Widgets/Tooltip.h"
#include "Aquila/UI/Style/ComputedStyle.h"

namespace Aquila::UI::Core {

Tooltip::Tooltip() : FloatingOverlay(48) {
	add_class("tooltip");
	set_dismiss_on_click_away(false);

	auto label = std::make_unique<Label>("");
	label->add_class("tooltip-label");
	m_label = add_child(std::move(label));
}

void Tooltip::show_at(Vec2 canvas_pos, std::string text) {
	m_label->set_text(std::move(text));

	FloatingConfig fc;
	fc.attach_to = FloatingAttachTo::Root;
	fc.element_point = FloatingAttachPoint::LeftTop;
	fc.parent_point = FloatingAttachPoint::LeftTop;
	fc.offset = canvas_pos;
	fc.z_index = 100;
	set_floating(fc);

	open();
}

void Tooltip::hide() {
	close();
}

Vec2 Tooltip::measure(const std::string &text) {
	m_label->set_text(text);
	const Vec2 text_size = m_label->measure();
	const ComputedStyle &cs = get_computed_style();
	const float pad_x = cs.padding.left.resolve(0.F) + cs.padding.right.resolve(0.F);
	const float pad_y = cs.padding.top.resolve(0.F) + cs.padding.bottom.resolve(0.F);
	return { text_size.x + pad_x, text_size.y + pad_y };
}

} // namespace Aquila::UI::Core
