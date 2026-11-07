#pragma once

#include "Aquila/Foundation/Reflection/TypeInfo.h"
#include "Aquila/Graphics/Material/Material.h"

namespace Aquila::GFX {
class GfxContext;
}

namespace Aquila::UI::Core {
class PropertyGrid;
class ReflectedPropertyGrid;
}

namespace Editor {

/// A row per parameter of a material's shader, bound to the material: edits go straight into it. Shared by
/// MaterialEditorRows and ShaderAssetContent.
class MaterialParameterRows {
  public:
	/// `read_only` shows the values without letting them be edited and leaves out Emit Light, a material setting
	/// rather than a shader parameter.
	MaterialParameterRows(Aquila::UI::Core::PropertyGrid &parent, Aquila::GFX::GfxContext *context, bool read_only);
	~MaterialParameterRows();

	/// Rebuilds the rows when the parameters changed. Null hides them. The rows point at the material until the next
	/// show(), keep it alive. `on_edited` runs after every edit.
	void show(Aquila::Graphics::Material *material, Delegate<void()> on_edited = {});
	void refresh();
	void set_hidden(bool hidden);

  private:
	Aquila::GFX::GfxContext *m_context = nullptr;
	bool m_read_only = false;
	Aquila::UI::Core::PropertyGrid *m_grid = nullptr;
	Aquila::Graphics::Material *m_material = nullptr;
	std::vector<std::pair<std::string, Aquila::Graphics::ParameterType>> m_shown;
	Unique<Aquila::Reflection::TypeInfo> m_type;
	Unique<Aquila::UI::Core::ReflectedPropertyGrid> m_fields;
};

}
