#include "Aquila/UI/Widgets/VecField.h"

namespace Aquila::UI::Core {

LabeledDragFloat::LabeledDragFloat(const char *label) {
	add_class("labeled-drag");

	auto lbl = std::make_unique<Label>(label);
	lbl->add_class("vec-label");
	m_label = add_child(std::move(lbl));

	auto drag = std::make_unique<DragFloat>();
	drag->add_class("vec-drag");
	m_drag = add_child(std::move(drag));
}

Vec2Field::Vec2Field() {
	add_class("vec2-field");

	auto xf = std::make_unique<LabeledDragFloat>("X");
	auto yf = std::make_unique<LabeledDragFloat>("Y");

	register_component(0, xf->get_drag());
	register_component(1, yf->get_drag());

	add_child(std::move(xf));
	add_child(std::move(yf));
}

Vec3Field::Vec3Field() {
	auto make_drag = [](const char *prefix, const char *axis_class) {
		auto drag = std::make_unique<DragFloat>();
		drag->set_prefix(prefix);
		drag->add_class("vec-drag");
		drag->add_class(axis_class);
		return drag;
	};

	auto x_drag = make_drag("X ", "vec-drag-x");
	auto y_drag = make_drag("Y ", "vec-drag-y");
	auto z_drag = make_drag("Z ", "vec-drag-z");

	register_component(0, add_child(std::move(x_drag)));
	register_component(1, add_child(std::move(y_drag)));
	register_component(2, add_child(std::move(z_drag)));
}

Vec4Field::Vec4Field() {
	add_class("vec4-field");

	auto xf = std::make_unique<LabeledDragFloat>("X");
	auto yf = std::make_unique<LabeledDragFloat>("Y");
	auto zf = std::make_unique<LabeledDragFloat>("Z");
	auto wf = std::make_unique<LabeledDragFloat>("W");

	register_component(0, xf->get_drag());
	register_component(1, yf->get_drag());
	register_component(2, zf->get_drag());
	register_component(3, wf->get_drag());

	add_child(std::move(xf));
	add_child(std::move(yf));
	add_child(std::move(zf));
	add_child(std::move(wf));
}

} // namespace Aquila::UI::Core
