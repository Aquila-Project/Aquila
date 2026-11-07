#include "UI/Inspector/Material/MaterialEditorRows.h"

#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "UI/Inspector/Material/MaterialParameterRows.h"

namespace Editor {

using namespace Aquila;
using Reflection::PropertyKind;
using Reflection::PropertyValue;

namespace {

constexpr Int32 K_SHADER_LIT = 0;
constexpr Int32 K_SHADER_UNLIT = 1;
constexpr Int32 K_SHADER_CUSTOM = 2;

bool is_builtin(const std::string &shader) {
	return shader == Graphics::STANDARD_SHADER || shader == Graphics::UNLIT_SHADER;
}

} // namespace

MaterialEditorRows::MaterialEditorRows(UI::Core::PropertyGrid &parent, GFX::GfxContext *context) : m_context(context) {
	m_settings_grid = parent.add_grid();
	m_settings_grid->set_split(parent.is_split());
	m_params = std::make_unique<MaterialParameterRows>(parent, context, false);
	build_settings();
}

MaterialEditorRows::~MaterialEditorRows() = default;

void MaterialEditorRows::build_settings() {
	m_settings_type = std::make_unique<Reflection::TypeInfo>("Material Settings");

	Reflection::Property shader;
	shader.name = "Shader";
	shader.kind = PropertyKind::Enum;
	shader.options = { { .name = "Lit (PBR)", .value = K_SHADER_LIT },
					   { .name = "Unlit", .value = K_SHADER_UNLIT },
					   { .name = "Custom Shader", .value = K_SHADER_CUSTOM } };
	shader.get = [](const void *instance) -> PropertyValue {
		const auto &rows = *static_cast<const MaterialEditorRows *>(instance);
		if (rows.m_custom || !is_builtin(rows.m_data.shader)) {
			return K_SHADER_CUSTOM;
		}
		return rows.m_data.shader == Graphics::UNLIT_SHADER ? K_SHADER_UNLIT : K_SHADER_LIT;
	};
	shader.set = [](void *instance, const PropertyValue &value) {
		auto &rows = *static_cast<MaterialEditorRows *>(instance);
		const Int32 choice = std::get<Int32>(value);
		if (choice == K_SHADER_CUSTOM) {
			rows.m_custom = true;
			return;
		}
		rows.m_custom = false;
		const std::string target(choice == K_SHADER_UNLIT ? Graphics::UNLIT_SHADER : Graphics::STANDARD_SHADER);
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

	const auto has_blending = [](const void *instance) {
		const auto &rows = *static_cast<const MaterialEditorRows *>(instance);
		return !rows.m_material || rows.m_material->get_domain() != Graphics::Shader::ShaderDomain::Fullscreen;
	};

	Reflection::Property blend_mode;
	blend_mode.name = "Blend Mode";
	blend_mode.kind = PropertyKind::Enum;
	blend_mode.options = { { .name = "Opaque", .value = static_cast<Int32>(Graphics::BlendMode::Opaque) },
						   { .name = "Masked", .value = static_cast<Int32>(Graphics::BlendMode::Masked) },
						   { .name = "Transparent", .value = static_cast<Int32>(Graphics::BlendMode::Transparent) } };
	blend_mode.get = [](const void *instance) -> PropertyValue {
		return static_cast<Int32>(static_cast<const MaterialEditorRows *>(instance)->m_data.blend_mode);
	};
	blend_mode.set = [](void *instance, const PropertyValue &value) {
		auto &rows = *static_cast<MaterialEditorRows *>(instance);
		const auto mode = static_cast<Graphics::BlendMode>(std::get<Int32>(value));
		if (mode != rows.m_data.blend_mode) {
			rows.m_data.blend_mode = mode;
			rows.notify(true);
		}
	};
	blend_mode.visible = has_blending;
	m_settings_type->add(std::move(blend_mode));

	Reflection::Property alpha_cutoff;
	alpha_cutoff.name = "Alpha Cutoff";
	alpha_cutoff.kind = PropertyKind::Float;
	alpha_cutoff.hints.min = 0.F;
	alpha_cutoff.hints.max = 1.F;
	alpha_cutoff.hints.speed = 0.01F;
	alpha_cutoff.hints.precision = 2;
	alpha_cutoff.hints.slider = true;
	alpha_cutoff.get = [](const void *instance) -> PropertyValue {
		return static_cast<const MaterialEditorRows *>(instance)->m_data.alpha_cutoff;
	};
	alpha_cutoff.set = [](void *instance, const PropertyValue &value) {
		auto &rows = *static_cast<MaterialEditorRows *>(instance);
		const F32 cutoff = std::clamp(std::get<F32>(value), 0.F, 1.F);
		if (cutoff == rows.m_data.alpha_cutoff) {
			return;
		}
		rows.m_data.alpha_cutoff = cutoff;
		if (rows.m_material) {
			rows.m_material->set_alpha_cutoff(cutoff);
		}
		rows.notify(false);
	};
	alpha_cutoff.visible = [has_blending](const void *instance) {
		return has_blending(instance) &&
			static_cast<const MaterialEditorRows *>(instance)->m_data.blend_mode == Graphics::BlendMode::Masked;
	};
	m_settings_type->add(std::move(alpha_cutoff));

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
	m_params->show(m_material.get(), [this] { notify(false); });
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
	m_params->refresh();
}

void MaterialEditorRows::set_hidden(bool hidden) {
	m_settings_grid->set_hidden(hidden);
	m_params->set_hidden(hidden);
}

} // namespace Editor
