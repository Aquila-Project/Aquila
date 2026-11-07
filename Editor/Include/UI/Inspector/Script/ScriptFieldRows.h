#pragma once

#include "Aquila/Foundation/Reflection/TypeInfo.h"
#include "Aquila/Scripting/ScriptFields.h"

namespace Aquila::SceneManagement::Components {
struct ScriptComponent;
}

namespace Aquila::UI::Core {
class PropertyGrid;
class ReflectedPropertyGrid;
}

namespace Editor {

class EditorContext;

/// A row per field a script declares, bound to a ScriptComponent: an entity's, where edits become its overrides, or
/// a scratch one with no overrides, which shows the file's defaults. Shared by ScriptContent and ScriptAssetContent.
class ScriptFieldRows {
  public:
	/// `read_only` shows the values without letting them be edited.
	ScriptFieldRows(Aquila::UI::Core::PropertyGrid &parent, EditorContext &context, bool read_only);
	~ScriptFieldRows();

	/// Reads the fields `component->path` declares (runs the script's top level in a scratch VM) and rebuilds the rows
	/// when they changed. Null hides them. `on_edited` runs after every edit.
	void show(Aquila::SceneManagement::Components::ScriptComponent *component, Delegate<void()> on_edited = {});
	void refresh();

  private:
	EditorContext &m_context;
	bool m_read_only = false;
	Aquila::UI::Core::PropertyGrid *m_grid = nullptr;
	std::vector<Aquila::Scripting::ScriptField> m_declared;
	Unique<Aquila::Reflection::TypeInfo> m_type;
	Unique<Aquila::UI::Core::ReflectedPropertyGrid> m_fields;
};

}
