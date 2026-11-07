#pragma once

#include "Aquila/Scene/Entity.h"
#include "Aquila/UI/Widgets/Inspectable.h"

namespace Aquila::UI::Core {
class Button;
}

namespace Editor {

class EditorContext;
class ScriptFieldRows;

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
	void open_script() const;

	Aquila::SceneManagement::Entity m_entity;
	EditorContext &m_context;
	Aquila::UI::Core::Button *m_open_button = nullptr;
	Unique<ScriptFieldRows> m_rows;
};

}
