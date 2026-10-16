#include "Aquila/UI/Widgets/ReflectedPropertyGrid.h"

#include "Aquila/UI/Widgets/AssetSlot.h"
#include "Aquila/UI/Widgets/Checkbox.h"
#include "Aquila/UI/Widgets/ColorPicker.h"
#include "Aquila/UI/Widgets/DragFloat.h"
#include "Aquila/UI/Widgets/DragInt.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Aquila/UI/Widgets/Toggle.h"
#include "Aquila/UI/Widgets/VecField.h"

namespace Aquila::UI::Core {

using Reflection::Property;
using Reflection::PropertyKind;
using Reflection::PropertyValue;

namespace {

DragFloat::Config drag_config(const Reflection::PropertyHints &hints) {
	return { .min = hints.min, .max = hints.max, .speed = hints.speed, .precision = hints.precision };
}

}

ReflectedPropertyGrid::ReflectedPropertyGrid(PropertyGrid &grid, const Reflection::TypeInfo &type,
											 GFX::GfxContext *ctx) {
	m_rows.reserve(type.get_properties().size());
	for (const Property &property : type.get_properties()) {
		add_row(grid, property, ctx);
	}
}

ReflectedPropertyGrid::~ReflectedPropertyGrid() = default;

void ReflectedPropertyGrid::bind(void *instance, Delegate<void()> on_edited) {
	m_instance = instance;
	m_on_edited = std::move(on_edited);
	refresh();
}

void ReflectedPropertyGrid::refresh() {
	if (m_instance == nullptr) {
		return;
	}

	m_refreshing = true;
	for (Row &row : m_rows) {
		row.show(row.property->get(m_instance));
		row.widget->get_parent()->set_hidden(!row.property->is_visible(m_instance));
	}
	m_refreshing = false;
}

View *ReflectedPropertyGrid::get_widget(std::string_view property_name) const {
	for (const Row &row : m_rows) {
		if (row.property->name == property_name) {
			return row.widget;
		}
	}
	return nullptr;
}

void ReflectedPropertyGrid::on_widget_edited(size_t row_index, const PropertyValue &value) {
	if (m_refreshing || m_instance == nullptr) {
		return;
	}

	m_rows[row_index].property->set(m_instance, value);
	if (m_on_edited) {
		m_on_edited();
	}

	for (const Row &row : m_rows) {
		row.widget->get_parent()->set_hidden(!row.property->is_visible(m_instance));
	}
}

void ReflectedPropertyGrid::add_row(PropertyGrid &grid, const Property &property, GFX::GfxContext *ctx) {
	const size_t index = m_rows.size();
	Row row;
	row.property = &property;

	auto edited = [this, index](const PropertyValue &value) { on_widget_edited(index, value); };
	const bool split = grid.is_split();

	switch (property.kind) {
	case PropertyKind::Bool: {
		auto connect = [&](auto *field) {
			field->on_changed.connect([edited](const bool &value) { edited(PropertyValue{ value }); });
			row.widget = field;
			row.show = [field](const PropertyValue &value) { field->set_value_without_notify(std::get<bool>(value)); };
		};
		if (split) {
			connect(grid.add_check_row<Checkbox>(property.name, false));
		} else if (property.hints.toggle) {
			connect(grid.add_row<Toggle>(property.name, false));
		} else {
			connect(grid.add_row<Checkbox>(property.name, false));
		}
		break;
	}
	case PropertyKind::Int: {
		auto *field = grid.add_row<DragInt>(property.name);
		field->set_speed(property.hints.speed);
		field->on_changed.connect(
			[edited](float value) { edited(PropertyValue{ static_cast<Int32>(std::lround(value)) }); });
		row.widget = field;
		row.show = [field](const PropertyValue &value) { field->set_int_value(std::get<Int32>(value)); };
		break;
	}
	case PropertyKind::Float: {
		auto *field = grid.add_row<DragFloat>(property.name, drag_config(property.hints));
		field->set_slider(property.hints.slider);
		field->set_suffix(std::string(property.hints.unit));
		field->on_changed.connect([edited](float value) { edited(PropertyValue{ value }); });
		row.widget = field;
		row.show = [field](const PropertyValue &value) { field->set_value(std::get<F32>(value)); };
		break;
	}
	case PropertyKind::Vec2: {
		auto *field = grid.add_stacked_row<Vec2Field>(property.name);
		field->set_speed(property.hints.speed);
		field->set_stacked(split);
		field->on_changed.connect([edited](Vec2 value) { edited(PropertyValue{ value }); });
		row.widget = field;
		row.show = [field](const PropertyValue &value) { field->set_value(std::get<Vec2>(value)); };
		break;
	}
	case PropertyKind::Vec3: {
		if (property.hints.color && ctx != nullptr) {
			auto *field = grid.add_row<ColorPicker>(property.name, *ctx, Vec4(1.F));
			field->on_changed.connect([edited](Vec4 value) { edited(PropertyValue{ Vec3(value) }); });
			row.widget = field;
			row.show = [field](const PropertyValue &value) { field->set_value(Vec4(std::get<Vec3>(value), 1.F)); };
		} else {
			auto *field = grid.add_stacked_row<Vec3Field>(property.name);
			field->set_speed(property.hints.speed);
			field->set_stacked(split);
			field->set_suffix(std::string(property.hints.unit));
			if (split) {
				field->set_precision(property.hints.precision);
			}
			field->on_changed.connect([edited](Vec3 value) { edited(PropertyValue{ value }); });
			row.widget = field;
			row.show = [field](const PropertyValue &value) { field->set_value(std::get<Vec3>(value)); };
		}
		break;
	}
	case PropertyKind::Vec4: {
		if (property.hints.color && ctx != nullptr) {
			auto *field = grid.add_row<ColorPicker>(property.name, *ctx, Vec4(1.F));
			field->on_changed.connect([edited](Vec4 value) { edited(PropertyValue{ value }); });
			row.widget = field;
			row.show = [field](const PropertyValue &value) { field->set_value(std::get<Vec4>(value)); };
		} else {
			auto *field = grid.add_stacked_row<Vec4Field>(property.name);
			field->set_speed(property.hints.speed);
			field->set_stacked(split);
			field->on_changed.connect([edited](Vec4 value) { edited(PropertyValue{ value }); });
			row.widget = field;
			row.show = [field](const PropertyValue &value) { field->set_value(std::get<Vec4>(value)); };
		}
		break;
	}
	case PropertyKind::String: {
		if (property.hints.read_only) {
			auto *label = grid.add_row<Label>(property.name, std::string{});
			row.widget = label;
			row.show = [label](const PropertyValue &value) { label->set_text(std::get<std::string>(value)); };
			break;
		}
		if (!property.hints.asset_type.empty()) {
			const std::string type(property.hints.asset_type);
			auto *slot = grid.add_row<AssetSlot>(property.name, type);
			slot->on_changed.connect([edited](const Option<AssetPayload> &asset) {
				edited(PropertyValue{ asset ? asset->asset_path : std::string{} });
			});
			row.widget = slot;
			row.show = [slot, type](const PropertyValue &value) {
				const auto &path = std::get<std::string>(value);
				if (path.empty()) {
					slot->clear();
					return;
				}
				slot->set_value(AssetPayload{ path, type, path.substr(path.find_last_of('/') + 1) });
			};
			break;
		}
		auto *field = grid.add_row<TextInput>(property.name);
		field->on_changed.connect([edited](const std::string &value) { edited(PropertyValue{ value }); });
		row.widget = field;
		row.show = [field](const PropertyValue &value) { field->set_text(std::get<std::string>(value)); };
		break;
	}
	case PropertyKind::Enum: {
		auto *field = grid.add_row<Dropdown>(property.name);
		for (const auto &option : property.options) {
			field->add_option(std::to_string(option.value), option.name);
		}
		field->on_changed.connect(
			[edited](const std::string &value) { edited(PropertyValue{ static_cast<Int32>(std::stoi(value)) }); });
		row.widget = field;
		row.show = [field](const PropertyValue &value) { field->set_value(std::to_string(std::get<Int32>(value))); };
		break;
	}
	}

	if (property.hints.read_only && property.kind != PropertyKind::String) {
		row.widget->set_enabled(false);
	}
	m_rows.push_back(std::move(row));
}

} // namespace Aquila::UI::Core
