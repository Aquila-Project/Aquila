#pragma once

#include "Aquila/Foundation/Reflection/TypeInfo.h"
#include "Aquila/Graphics/Material/Material.h"
#include "Aquila/UI/Widgets/ReflectedPropertyGrid.h"

namespace Aquila::GFX {
class GfxContext;
}

namespace Aquila::UI::Core {
class Label;
class PropertyGrid;
} // namespace Aquila::UI::Core

namespace Editor {

/// Shader, Double Sided and Blend Mode report `needs_rebuild`: the owner makes a new material and calls show()
/// again, which is safe from inside the callback. Other edits go straight into the material.
class MaterialEditorRows {
  public:
	using ChangedCallback = Delegate<void(const Aquila::Graphics::MaterialData &data, bool needs_rebuild)>;

	MaterialEditorRows(Aquila::UI::Core::PropertyGrid &parent, Aquila::GFX::GfxContext *context);
	~MaterialEditorRows();

	void show(Ref<Aquila::Graphics::Material> material, ChangedCallback on_changed);
	void refresh();
	void set_hidden(bool hidden);

  private:
	void build_settings();
	void build_parameters();
	void notify(bool needs_rebuild);

	Aquila::GFX::GfxContext *m_context = nullptr;
	Aquila::UI::Core::PropertyGrid *m_settings_grid = nullptr;
	Aquila::UI::Core::PropertyGrid *m_params_grid = nullptr;
	Aquila::UI::Core::Label *m_kind_label = nullptr;
	Unique<Aquila::Reflection::TypeInfo> m_settings_type;
	Unique<Aquila::UI::Core::ReflectedPropertyGrid> m_settings;
	Unique<Aquila::Reflection::TypeInfo> m_params_type;
	Unique<Aquila::UI::Core::ReflectedPropertyGrid> m_params;
	std::vector<std::pair<std::string, Aquila::Graphics::ParameterType>> m_shown_parameters;
	Ref<Aquila::Graphics::Material> m_material;
	Aquila::Graphics::MaterialData m_data;
	bool m_custom = false;
	ChangedCallback m_on_changed;
};

} // namespace Editor
