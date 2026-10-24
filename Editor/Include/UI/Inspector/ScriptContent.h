#pragma once

#include "Aquila/Foundation/Reflection/TypeInfo.h"
#include "Aquila/Scene/Entity.h"
#include "Aquila/Scripting/ScriptFields.h"
#include "Aquila/UI/Widgets/Inspectable.h"

namespace Aquila::UI::Core {
class ReflectedPropertyGrid;
}

namespace Editor {

class EditorContext;

/// Fields are read again when the section is built or the path changes, not when the file changes on disk.
class ScriptContent final : public Aquila::UI::Core::IInspectorContent {
  public:
	ScriptContent(Aquila::SceneManagement::Entity entity, EditorContext &context);
	~ScriptContent() override;

	void build_below(Aquila::UI::Core::PropertyGrid &grid) override;
	void refresh() override;
	void on_property_edited() override;

  private:
	void show_fields();

	Aquila::SceneManagement::Entity m_entity;
	EditorContext &m_context;
	Aquila::UI::Core::PropertyGrid *m_fields_grid = nullptr;
	Unique<Aquila::UI::Core::ReflectedPropertyGrid> m_fields;
	Unique<Aquila::Reflection::TypeInfo> m_fields_type;
	std::vector<Aquila::Scripting::ScriptField> m_declared;
};

}
