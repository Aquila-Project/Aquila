#include "UI/Inspector/Material/MaterialParameterRows.h"

#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/ReflectedPropertyGrid.h"
#include "UI/Inspector/Common/InspectorLabels.h"

namespace Editor {

using namespace Aquila;
using Graphics::MaterialParameter;
using Graphics::MaterialValue;
using Graphics::ParameterType;
using Graphics::TextureReference;
using Reflection::PropertyKind;
using Reflection::PropertyValue;

namespace {

constexpr std::array<Reflection::PropertyMark, 6> K_LUMINANCE_MARKS = { {
	{ .value = 80.F, .label = "Monitor white" },
	{ .value = 500.F, .label = "Phone screen" },
	{ .value = 12000.F, .label = "Fluorescent tube" },
	{ .value = 75000.F, .label = "Sodium street lamp" },
	{ .value = 130000.F, .label = "Frosted bulb" },
	{ .value = 7000000.F, .label = "Bulb filament" },
} };

bool is_emission_strength(const MaterialParameter &param) {
	return param.type == ParameterType::Float && Graphics::is_emission_strength_name(param.name);
}

PropertyKind kind_of(ParameterType type) {
	switch (type) {
	case ParameterType::Float:
		return PropertyKind::Float;
	case ParameterType::Int:
		return PropertyKind::Int;
	case ParameterType::Bool:
		return PropertyKind::Bool;
	case ParameterType::Vec2:
		return PropertyKind::Vec2;
	case ParameterType::Vec3:
		return PropertyKind::Vec3;
	case ParameterType::Vec4:
	case ParameterType::Color:
		return PropertyKind::Vec4;
	case ParameterType::Texture2D:
		return PropertyKind::String;
	}
	return PropertyKind::Float;
}

PropertyValue empty_value(PropertyKind kind) {
	switch (kind) {
	case PropertyKind::Bool:
		return false;
	case PropertyKind::Int:
	case PropertyKind::Enum:
		return Int32{ 0 };
	case PropertyKind::Vec2:
		return Vec2(0.F);
	case PropertyKind::Vec3:
		return Vec3(0.F);
	case PropertyKind::Vec4:
		return Vec4(0.F);
	case PropertyKind::String:
		return std::string();
	case PropertyKind::Float:
		break;
	}
	return 0.F;
}

PropertyValue to_property(const MaterialValue &value) {
	return std::visit(
		[](const auto &stored) -> PropertyValue {
			using V = std::decay_t<decltype(stored)>;
			if constexpr (std::is_same_v<V, TextureReference>) {
				return stored.path;
			} else {
				return stored;
			}
		},
		value);
}

MaterialValue from_property(const PropertyValue &value, ParameterType type) {
	if (type == ParameterType::Texture2D) {
		const auto *path = std::get_if<std::string>(&value);
		return TextureReference{ path != nullptr ? *path : std::string() };
	}
	return std::visit(
		[](const auto &stored) -> MaterialValue {
			using V = std::decay_t<decltype(stored)>;
			if constexpr (std::is_same_v<V, std::string>) {
				return TextureReference{ stored };
			} else {
				return stored;
			}
		},
		value);
}

Reflection::Property make_parameter_property(const MaterialParameter &param, bool read_only) {
	Reflection::Property property;
	property.name = label_of(param.name);
	property.kind = kind_of(param.type);
	property.hints.read_only = read_only;

	switch (param.type) {
	case ParameterType::Float: {
		property.hints.speed = 0.01F;
		property.hints.precision = 3;
		if (Graphics::is_unit_range_name(param.name)) {
			property.hints.min = 0.F;
			property.hints.max = 1.F;
			property.hints.slider = true;
		} else if (is_emission_strength(param)) {
			property.hints.min = 0.F;
			property.hints.max = 2000000000.F;
			property.hints.precision = 1;
			property.hints.unit = "nits";
			property.hints.logarithmic = true;
			property.hints.slider_min = 1.F;
			property.hints.slider_max = 10000000.F;
			property.hints.marks = K_LUMINANCE_MARKS;
		} else if (Foundation::contains_ignore_case(param.name, "intensity")) {
			property.hints.min = 0.F;
			property.hints.speed = 1.F;
			property.hints.precision = 1;
			property.hints.unit = "nits";
		}
		break;
	}
	case ParameterType::Int:
		property.hints.speed = 1.F;
		break;
	case ParameterType::Vec2:
		property.hints.speed = 0.01F;
		property.hints.precision = 3;
		break;
	case ParameterType::Vec3:
	case ParameterType::Vec4:
	case ParameterType::Color:
		property.hints.color = Graphics::is_color_name(param.name);
		property.hints.speed = 0.01F;
		property.hints.precision = 3;
		break;
	case ParameterType::Texture2D:
		property.hints.asset_type = "texture";
		break;
	case ParameterType::Bool:
		break;
	}

	const std::string name = param.name;
	const ParameterType type = param.type;
	const PropertyKind kind = property.kind;
	property.get = [name, kind](const void *instance) -> PropertyValue {
		const auto &material = *static_cast<const Graphics::Material *>(instance);
		const MaterialParameter *found = material.get_parameter(name);
		return found != nullptr ? to_property(material.get_value(*found)) : empty_value(kind);
	};
	property.set = [name, type](void *instance, const PropertyValue &value) {
		static_cast<Graphics::Material *>(instance)->set_value(name, from_property(value, type));
	};
	return property;
}

Reflection::Property make_emit_light_property() {
	Reflection::Property property;
	property.name = "Emit Light";
	property.kind = PropertyKind::Bool;
	property.hints.toggle = true;
	property.get = [](const void *instance) -> PropertyValue {
		return static_cast<const Graphics::Material *>(instance)->get_data().emit_light;
	};
	property.set = [](void *instance, const PropertyValue &value) {
		static_cast<Graphics::Material *>(instance)->set_emit_light(std::get<bool>(value));
	};
	return property;
}

}

MaterialParameterRows::MaterialParameterRows(UI::Core::PropertyGrid &parent, GFX::GfxContext *context, bool read_only)
	: m_context(context), m_read_only(read_only), m_grid(parent.add_grid()) {
	if (m_grid != nullptr) {
		m_grid->set_split(parent.is_split());
	}
}

MaterialParameterRows::~MaterialParameterRows() = default;

void MaterialParameterRows::show(Graphics::Material *material, Delegate<void()> on_edited) {
	m_material = material;
	if (m_grid == nullptr) {
		return;
	}
	std::vector<std::pair<std::string, ParameterType>> visible;
	if (m_material != nullptr) {
		for (const MaterialParameter &param : m_material->get_parameters()) {
			if (!param.hidden) {
				visible.emplace_back(param.name, param.type);
			}
		}
	}

	if (m_fields == nullptr || visible != m_shown) {
		m_fields.reset();
		m_grid->clear_children();
		m_shown = std::move(visible);
		m_type = std::make_unique<Reflection::TypeInfo>("Material Parameters");
		if (m_material != nullptr) {
			const bool emits = !m_read_only && m_material->get_domain() == Graphics::Shader::ShaderDomain::Lit &&
				m_material->get_average_emission().has_value();
			bool emit_light_added = false;
			for (const MaterialParameter &param : m_material->get_parameters()) {
				if (param.hidden) {
					continue;
				}
				m_type->add(make_parameter_property(param, m_read_only));
				if (emits && !emit_light_added && is_emission_strength(param)) {
					m_type->add(make_emit_light_property());
					emit_light_added = true;
				}
			}
			if (emits && !emit_light_added) {
				m_type->add(make_emit_light_property());
			}
		}
		m_fields = std::make_unique<UI::Core::ReflectedPropertyGrid>(*m_grid, *m_type, m_context);
	}

	m_grid->set_hidden(m_material == nullptr || m_shown.empty());
	if (m_material != nullptr) {
		m_fields->bind(m_material, std::move(on_edited));
	}
}

void MaterialParameterRows::refresh() {
	if (m_fields != nullptr) {
		m_fields->refresh();
	}
}

void MaterialParameterRows::set_hidden(bool hidden) {
	if (m_grid != nullptr) {
		m_grid->set_hidden(hidden || m_material == nullptr || m_shown.empty());
	}
}

}
