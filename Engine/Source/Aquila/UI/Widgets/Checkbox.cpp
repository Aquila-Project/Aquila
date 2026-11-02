#include "Aquila/UI/Widgets/Checkbox.h"

#include "Aquila/UI/Core/MarkupItem.h"

namespace Aquila::UI::Core {

Checkbox::Checkbox() {
	set_input_leaf(true);
}

Checkbox::Checkbox(bool checked) {
	set_input_leaf(true);
	set_value_without_notify(checked);
}

void Checkbox::on_value_updated() {
	queue_redraw();
}

void Checkbox::apply_xml_attribute(std::string_view name, std::string_view value, IResourceResolver *resolver) {
	if (name != "checked") {
		BaseField::apply_xml_attribute(name, value, resolver);
		return;
	}
	if (const Option<bool> checked = parse_markup_bool(value)) {
		set_value_without_notify(*checked);
	} else {
		AQUILA_LOG_WARNING("Checkbox: checked expects true or false, got '{}'", value);
	}
}

void Checkbox::on_mouse_release(Platform::MouseButton btn, Vec2 pos) {
	if (btn == Platform::MouseButton::Left && m_is_hovered) {
		set_value(!get_value());
	}
	View::on_mouse_release(btn, pos);
}

void Checkbox::on_draw_self(Rendering::DrawList &draw_list) {
	View::on_draw_self(draw_list); // draws background + border from style

	if (!get_value()) {
		return;
	}

	const Rect rect = get_absolute_rect();
	const auto &style = get_display_style();
	constexpr float pad = 4.F;
	const Int32 z = 2;
	const Rect fill = {
		.position = rect.position + Vec2(pad),
		.size = rect.size - Vec2(pad * 2.F),
	};
	draw_list.draw_rect(fill, style.color, style.border_radius, 0.F, Vec4(0.F), z);
}

} // namespace Aquila::UI::Core
