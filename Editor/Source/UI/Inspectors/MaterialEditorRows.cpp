#include "UI/Inspectors/MaterialEditorRows.h"

#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "UI/Inspectors/InspectorLabels.h"

namespace Editor {

using namespace Aquila;
using Graphics::MaterialParameter;
using Graphics::MaterialValue;
using Graphics::ParameterType;
using Graphics::TextureReference;
using Reflection::PropertyKind;
using Reflection::PropertyValue;

namespace {

constexpr Int32 k_shader_lit = 0;
constexpr Int32 k_shader_unlit = 1;
constexpr Int32 k_shader_custom = 2;

bool name_has(std::string_view name, std::string_view word) {
	std::string lowered(name);
	std::ranges::transform(lowered, lowered.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lowered.find(word) != std::string::npos;
}

bool is_color_name(std::string_view name) {
	return name_has(name, "color") || name_has(name, "colour") || name_has(name, "tint") || name_has(name, "albedo");
}

bool is_builtin(const std::string &shader) {
	return shader == Graphics::STANDARD_SHADER || shader == Graphics::UNLIT_SHADER;
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

Reflection::Property make_parameter_property(const MaterialParameter &param) {
	Reflection::Property property;
	property.name = label_of(param.name);
	property.kind = kind_of(param.type);

	switch (param.type) {
	case ParameterType::Float: {
		const bool unit_range = name_has(param.name, "metallic") || name_has(param.name, "roughness") ||
			name_has(param.name, "occlusion") || name_has(param.name, "opacity") || name_has(param.name, "alpha");
		property.hints.speed = 0.01F;
		property.hints.precision = 3;
		if (unit_range) {
			property.hints.min = 0.F;
			property.hints.max = 1.F;
			property.hints.slider = true;
		} else if (name_has(param.name, "intensity")) {
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
		property.hints.color = is_color_name(param.name);
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

} // namespace

MaterialEditorRows::MaterialEditorRows(UI::Core::PropertyGrid &parent, GFX::GfxContext *context) : m_context(context) {
	m_settings_grid = parent.add_child<UI::Core::PropertyGrid>(parent.get_label_width());
	m_settings_grid->set_split(parent.is_split());
	m_params_grid = parent.add_child<UI::Core::PropertyGrid>(parent.get_label_width());
	m_params_grid->set_split(parent.is_split());
	build_settings();
}

MaterialEditorRows::~MaterialEditorRows() = default;

void MaterialEditorRows::build_settings() {
	m_settings_type = std::make_unique<Reflection::TypeInfo>("Material Settings");

	Reflection::Property shader;
	shader.name = "Shader";
	shader.kind = PropertyKind::Enum;
	shader.options = { { .name = "Lit (PBR)", .value = k_shader_lit },
					   { .name = "Unlit", .value = k_shader_unlit },
					   { .name = "Custom Shader", .value = k_shader_custom } };
	shader.get = [](const void *instance) -> PropertyValue {
		const auto &rows = *static_cast<const MaterialEditorRows *>(instance);
		if (rows.m_custom || !is_builtin(rows.m_data.shader)) {
			return k_shader_custom;
		}
		return rows.m_data.shader == Graphics::UNLIT_SHADER ? k_shader_unlit : k_shader_lit;
	};
	shader.set = [](void *instance, const PropertyValue &value) {
		auto &rows = *static_cast<MaterialEditorRows *>(instance);
		const Int32 choice = std::get<Int32>(value);
		if (choice == k_shader_custom) {
			rows.m_custom = true;
			return;
		}
		rows.m_custom = false;
		const std::string target(choice == k_shader_unlit ? Graphics::UNLIT_SHADER : Graphics::STANDARD_SHADER);
		if (target != rows.m_data.shader) {
			rows.m_data.shader = target;
			rows.notify(true);
		}
	};
	m_settings_type->add(std::move(shader));

	Reflection::Property file;
	file.name = "Shader File";
	file.kind = PropertyKind::String;
	file.hints.asset_type = "shader";
	file.get = [](const void *instance) -> PropertyValue {
		const auto &rows = *static_cast<const MaterialEditorRows *>(instance);
		return is_builtin(rows.m_data.shader) ? std::string() : rows.m_data.shader;
	};
	file.set = [](void *instance, const PropertyValue &value) {
		auto &rows = *static_cast<MaterialEditorRows *>(instance);
		const std::string &path = std::get<std::string>(value);
		if (path.empty() || path == rows.m_data.shader) {
			return;
		}
		rows.m_data.shader = path;
		rows.m_custom = true;
		rows.notify(true);
	};
	file.visible = [](const void *instance) {
		const auto &rows = *static_cast<const MaterialEditorRows *>(instance);
		return rows.m_custom || !is_builtin(rows.m_data.shader);
	};
	m_settings_type->add(std::move(file));

	Reflection::Property double_sided;
	double_sided.name = "Double Sided";
	double_sided.kind = PropertyKind::Bool;
	double_sided.get = [](const void *instance) -> PropertyValue {
		return static_cast<const MaterialEditorRows *>(instance)->m_data.double_sided;
	};
	double_sided.set = [](void *instance, const PropertyValue &value) {
		auto &rows = *static_cast<MaterialEditorRows *>(instance);
		const bool enabled = std::get<bool>(value);
		if (enabled != rows.m_data.double_sided) {
			rows.m_data.double_sided = enabled;
			rows.notify(true);
		}
	};
	m_settings_type->add(std::move(double_sided));

	m_settings = std::make_unique<UI::Core::ReflectedPropertyGrid>(*m_settings_grid, *m_settings_type, m_context);
	m_kind_label = m_settings_grid->add_row<UI::Core::Label>("Kind", std::string{});
	m_settings->bind(this, {});
}

void MaterialEditorRows::show(Ref<Graphics::Material> material, ChangedCallback on_changed) {
	m_material = std::move(material);
	m_on_changed = std::move(on_changed);
	m_data = m_material ? m_material->get_data() : Graphics::MaterialData{};
	m_custom = !is_builtin(m_data.shader);

	if (m_kind_label != nullptr) {
		const std::string kind = m_material
			? std::string(Aquila::Graphics::Shader::domain_name(m_material->get_domain())) + " shader"
			: std::string("None");
		m_kind_label->set_text(kind);
	}
	m_settings->refresh();
	build_parameters();
}

void MaterialEditorRows::build_parameters() {
	std::vector<std::pair<std::string, ParameterType>> visible;
	if (m_material) {
		for (const MaterialParameter &param : m_material->get_parameters()) {
			if (!param.hidden) {
				visible.emplace_back(param.name, param.type);
			}
		}
	}

	if (m_params == nullptr || visible != m_shown_parameters) {
		m_params.reset();
		while (!m_params_grid->get_children().empty()) {
			m_params_grid->remove_child(m_params_grid->get_children().back().get());
		}
		m_shown_parameters = std::move(visible);
		m_params_type = std::make_unique<Reflection::TypeInfo>("Material Parameters");
		if (m_material) {
			for (const MaterialParameter &param : m_material->get_parameters()) {
				if (param.hidden) {
					continue;
				}
				m_params_type->add(make_parameter_property(param));
			}
		}
		m_params = std::make_unique<UI::Core::ReflectedPropertyGrid>(*m_params_grid, *m_params_type, m_context);
	}

	m_params_grid->set_hidden(!m_material || m_shown_parameters.empty());
	if (m_material) {
		m_params->bind(m_material.get(), [this] { notify(false); });
	}
}

void MaterialEditorRows::notify(bool needs_rebuild) {
	if (!needs_rebuild && m_material) {
		m_data = m_material->get_data();
	}
	ChangedCallback callback = m_on_changed;
	if (callback) {
		callback(m_data, needs_rebuild);
	}
}

void MaterialEditorRows::refresh() {
	m_settings->refresh();
	if (m_params != nullptr) {
		m_params->refresh();
	}
}

void MaterialEditorRows::set_hidden(bool hidden) {
	m_settings_grid->set_hidden(hidden);
	m_params_grid->set_hidden(hidden || !m_material || m_shown_parameters.empty());
}

} // namespace Editor
