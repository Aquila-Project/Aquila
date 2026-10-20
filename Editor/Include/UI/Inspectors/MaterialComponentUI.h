#pragma once

#include "UI/Inspectors/ReflectedComponentUI.h"

namespace Aquila::UI::Core {
class Button;
class Label;
class PropertyGrid;
}

namespace Editor {

class MaterialEditorRows;

/// Material section: the reflected Material slot (a .aqmat from the content browser, empty for the entity's own
/// material), a note saying which one is being edited, then MaterialEditorRows. Edits to a shared .aqmat are saved
/// to the file right away and change every entity that uses it. "Save as Asset" turns the entity's own material
/// into a .aqmat under Content/Materials.
class MaterialComponentUI : public ReflectedComponentUI {
  public:
	MaterialComponentUI(const Aquila::SceneManagement::ComponentDescriptor &descriptor,
						Aquila::GFX::GfxContext &context);
	~MaterialComponentUI() override;

	void build(Aquila::UI::Core::Collapsible *section, Aquila::UI::Core::PropertyGrid *grid) override;
	void show(Aquila::SceneManagement::Entity entity) override;
	void refresh() override;

  protected:
	void on_reflected_edit(Aquila::SceneManagement::Entity entity) override;

  private:
	void show_material(Aquila::SceneManagement::Entity entity);
	void save_as_asset(Aquila::SceneManagement::Entity entity);

	Aquila::GFX::GfxContext &m_context;
	Aquila::UI::Core::Label *m_note = nullptr;
	Aquila::UI::Core::Button *m_save_button = nullptr;
	Unique<MaterialEditorRows> m_rows;
	Aquila::SceneManagement::Entity m_entity;
};

} // namespace Editor
