#include "Aquila/UI/Widgets/PropertyGrid.h"

#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/UIResources.h"

namespace Aquila::UI::Core {

namespace {

constexpr std::string_view K_COMPONENTS = "property_grid";

std::string split_class(const View &widget) {
	return "split-" + std::string(widget.get_type_name());
}

}

PropertyGrid::PropertyGrid(float label_width) : m_label_width(label_width) {
	add_class("property-grid");
}

void PropertyGrid::set_split(bool split) {
	m_split = split;
	set_class("property-grid-split", split);
}

void PropertyGrid::apply_xml_attribute(std::string_view name, std::string_view value, IResourceResolver *resolver) {
	if (name == "split") {
		set_split(value == "true");
		return;
	}
	if (name != "label-width") {
		View::apply_xml_attribute(name, value, resolver);
		return;
	}
	F32 width = 0.F;
	const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), width);
	if (error != std::errc{} || end != value.data() + value.size()) {
		AQUILA_LOG_WARNING("PropertyGrid: label-width expects a number, got '{}'", value);
		return;
	}
	m_label_width = width;
}

View *PropertyGrid::add_row(std::string label, Unique<View> widget, RowLayout layout) {
	const bool stacked = layout == RowLayout::Stacked;
	if (m_split) {
		widget->add_class(split_class(*widget));
		return add_component_row(stacked ? "PropertySplitStackedRow" : "PropertySplitRow", std::move(label),
								 std::move(widget));
	}
	return add_component_row(stacked ? "PropertyStackedRow" : "PropertyRow", std::move(label), std::move(widget));
}

View *PropertyGrid::add_check_row(std::string label, Unique<View> widget) {
	if (!m_split) {
		return add_row(std::move(label), std::move(widget));
	}
	widget->add_class(split_class(*widget));
	return add_component_row("PropertySplitCheckRow", std::move(label), std::move(widget));
}

void PropertyGrid::add_separator() {
	if (LayoutLoader *loader = loader_for("separator")) {
		loader->create(engine_component_file(K_COMPONENTS), "PropertySeparator", *this);
	}
}

PropertyGrid *PropertyGrid::add_grid() {
	LayoutLoader *loader = loader_for("nested grid");
	if (loader == nullptr) {
		return nullptr;
	}
	auto *grid =
		dynamic_cast<PropertyGrid *>(loader->create(engine_component_file(K_COMPONENTS), "PropertyNestedGrid", *this));
	if (grid != nullptr) {
		grid->set_label_width(m_label_width);
	}
	return grid;
}

View *PropertyGrid::add_action(View &widget, Unique<View> action) {
	View *row = widget.get_parent();
	if (row == nullptr || row->get_parent() != this) {
		return nullptr;
	}
	return row->add_child(std::move(action));
}

View *PropertyGrid::add_component_row(std::string_view component, std::string label, Unique<View> widget) {
	LayoutLoader *loader = loader_for(label);
	if (loader == nullptr) {
		return nullptr;
	}
	View *placed = widget.get();
	ComponentArgs args{ .attributes = { { "label", std::move(label) } } };
	if (component == "PropertyRow") {
		args.attributes.emplace_back("width", std::format("{}px", m_label_width));
	}
	args.children.push_back(std::move(widget));
	return loader->create(engine_component_file(K_COMPONENTS), component, *this, std::move(args)) != nullptr ? placed
																											 : nullptr;
}

LayoutLoader *PropertyGrid::loader_for(std::string_view what) const {
	LayoutLoader *loader = find_layout_loader(*this);
	if (loader == nullptr) {
		AQUILA_LOG_ERROR("PropertyGrid: '{}' needs a canvas with a layout loader", what);
	}
	return loader;
}

} // namespace Aquila::UI::Core
