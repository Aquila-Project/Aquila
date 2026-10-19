#include "UI/Inspectors/ScriptComponentUI.h"

#include "Aquila/Scene/ComponentRegistry.h"
#include "Aquila/Scene/Components/ScriptComponent.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "UI/Inspectors/InspectorLabels.h"

namespace Editor {

using namespace Aquila;
using SceneManagement::Entity;
using SceneManagement::Components::EntityReference;
using SceneManagement::Components::ScriptComponent;
using SceneManagement::Components::ScriptFieldValue;
using Scripting::ScriptField;
using Scripting::ScriptFieldType;

namespace {

F32 to_limit(F64 value) {
	if (value <= static_cast<F64>(std::numeric_limits<F32>::lowest())) {
		return std::numeric_limits<F32>::lowest();
	}
	if (value >= static_cast<F64>(std::numeric_limits<F32>::max())) {
		return std::numeric_limits<F32>::max();
	}
	return static_cast<F32>(value);
}

Reflection::PropertyValue to_property(const ScriptField &field, const ScriptFieldValue &value) {
	return std::visit(
		[&field](const auto &stored) -> Reflection::PropertyValue {
			using V = std::decay_t<decltype(stored)>;
			if constexpr (std::is_same_v<V, F64>) {
				if (field.integer) {
					return static_cast<Int32>(std::lround(stored));
				}
				return static_cast<F32>(stored);
			} else if constexpr (std::is_same_v<V, EntityReference>) {
				return stored.id == Foundation::UUID::null() ? std::string() : stored.id.to_string();
			} else {
				return stored;
			}
		},
		value);
}

ScriptFieldValue from_property(const ScriptField &field, const Reflection::PropertyValue &value) {
	switch (field.type) {
	case ScriptFieldType::Number: {
		F64 number = field.integer ? static_cast<F64>(std::get<Int32>(value)) : static_cast<F64>(std::get<F32>(value));
		if (field.min <= field.max) {
			number = std::clamp(number, field.min, field.max);
		}
		return number;
	}
	case ScriptFieldType::Boolean:
		return std::get<bool>(value);
	case ScriptFieldType::String:
		return std::get<std::string>(value);
	case ScriptFieldType::Vector:
		return std::get<Vec3>(value);
	case ScriptFieldType::Entity: {
		const auto &id = std::get<std::string>(value);
		return EntityReference{ id.empty() ? Foundation::UUID::null() : Foundation::UUID::from_string(id) };
	}
	}
	return field.default_value;
}

Reflection::Property make_property(const ScriptField &field) {
	Reflection::Property property;
	property.name = label_of(field.name);
	switch (field.type) {
	case ScriptFieldType::Number: {
		property.kind = field.integer ? Reflection::PropertyKind::Int : Reflection::PropertyKind::Float;
		property.hints.min = to_limit(field.min);
		property.hints.max = to_limit(field.max);
		property.hints.speed = field.step > 0.0 ? static_cast<F32>(field.step) : (field.integer ? 1.F : 0.1F);
		property.hints.slider = field.slider && std::isfinite(field.min) && std::isfinite(field.max);
		break;
	}
	case ScriptFieldType::Boolean:
		property.kind = Reflection::PropertyKind::Bool;
		break;
	case ScriptFieldType::String:
		property.kind = Reflection::PropertyKind::String;
		break;
	case ScriptFieldType::Vector:
		property.kind = Reflection::PropertyKind::Vec3;
		property.hints.color = field.color;
		property.hints.speed = 0.05F;
		property.hints.precision = 3;
		break;
	case ScriptFieldType::Entity:
		property.kind = Reflection::PropertyKind::String;
		property.hints.asset_type = "entity";
		break;
	}

	property.get = [field](const void *instance) {
		return to_property(field, Scripting::field_value(field, *static_cast<const ScriptComponent *>(instance)));
	};
	property.set = [field](void *instance, const Reflection::PropertyValue &value) {
		auto &component = *static_cast<ScriptComponent *>(instance);
		ScriptFieldValue stored = from_property(field, value);
		if (stored == field.default_value) {
			component.fields.erase(field.name);
		} else {
			component.fields[field.name] = std::move(stored);
		}
	};
	return property;
}

}

ScriptComponentUI::ScriptComponentUI(const SceneManagement::ComponentDescriptor &descriptor, GFX::GfxContext *context)
	: ReflectedComponentUI(descriptor, context), m_context(context) {}

ScriptComponentUI::~ScriptComponentUI() = default;

void ScriptComponentUI::build(UI::Core::Collapsible *section, UI::Core::PropertyGrid *grid) {
	ReflectedComponentUI::build(section, grid);
	m_fields_grid = grid->add_child<UI::Core::PropertyGrid>(grid->get_label_width());
	m_fields_grid->set_split(grid->is_split());
	m_fields_grid->set_hidden(true);
}

void ScriptComponentUI::show(Entity entity) {
	ReflectedComponentUI::show(entity);
	m_entity = entity;
	show_fields(entity);
}

void ScriptComponentUI::refresh() {
	ReflectedComponentUI::refresh();
	if (m_fields != nullptr) {
		m_fields->refresh();
	}
}

void ScriptComponentUI::on_reflected_edit(Entity entity) {
	show_fields(entity);
}

void ScriptComponentUI::show_fields(Entity entity) {
	auto *component = entity.try_get_component<ScriptComponent>();
	std::vector<ScriptField> declared =
		component != nullptr ? Scripting::read_script_fields(component->path) : std::vector<ScriptField>{};

	if (m_fields == nullptr || declared != m_declared) {
		m_fields.reset();
		while (!m_fields_grid->get_children().empty()) {
			m_fields_grid->remove_child(m_fields_grid->get_children().back().get());
		}
		m_declared = std::move(declared);
		m_fields_type = std::make_unique<Reflection::TypeInfo>("Script Fields");
		for (const ScriptField &field : m_declared) {
			m_fields_type->add(make_property(field));
		}
		m_fields = std::make_unique<UI::Core::ReflectedPropertyGrid>(*m_fields_grid, *m_fields_type, m_context);
	}
	m_fields_grid->set_hidden(m_declared.empty() || component == nullptr);
	if (component == nullptr) {
		return;
	}
	m_fields->bind(component, [this, entity] {
		if (on_edited) {
			on_edited(entity);
		}
	});
}

}
