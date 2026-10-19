#pragma once

#include "Aquila/Foundation/Reflection/TypeInfo.h"
#include "Aquila/Scripting/ScriptFields.h"
#include "UI/Inspectors/ReflectedComponentUI.h"

namespace Editor {

/// Script section: the reflected Path slot, then the fields the script declares with Aquila.Field, built as a
/// TypeInfo over the ScriptComponent so ReflectedPropertyGrid edits them. Entity fields are "entity" asset slots,
/// filled by dragging a hierarchy row onto them (EditorModule registers that AssetSlot adapter).
///
/// @note The fields are read again (the script's top level runs in a scratch runtime) each time the section shows an
/// entity or the path changes, not when the file changes on disk: reselect to pick up an edit.
class ScriptComponentUI : public ReflectedComponentUI {
  public:
	ScriptComponentUI(const Aquila::SceneManagement::ComponentDescriptor &descriptor,
					  Aquila::GFX::GfxContext *context);
	~ScriptComponentUI() override;

	void build(Aquila::UI::Core::Collapsible *section, Aquila::UI::Core::PropertyGrid *grid) override;
	void show(Aquila::SceneManagement::Entity entity) override;
	void refresh() override;

  protected:
	void on_reflected_edit(Aquila::SceneManagement::Entity entity) override;

  private:
	void show_fields(Aquila::SceneManagement::Entity entity);

	Aquila::GFX::GfxContext *m_context = nullptr;
	Aquila::UI::Core::PropertyGrid *m_fields_grid = nullptr;
	Unique<Aquila::UI::Core::ReflectedPropertyGrid> m_fields;
	Unique<Aquila::Reflection::TypeInfo> m_fields_type;
	std::vector<Aquila::Scripting::ScriptField> m_declared;
	Aquila::SceneManagement::Entity m_entity;
};

}
