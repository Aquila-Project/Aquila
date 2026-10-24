#pragma once

#include "Aquila/Scene/Entity.h"
#include "Aquila/UI/Widgets/Inspectable.h"

namespace Aquila::UI::Core {
class Button;
class Label;
} // namespace Aquila::UI::Core

namespace Editor {

class EditorContext;
class MaterialEditorRows;

/// Edits to a shared .aqmat are saved to the file right away.
class MaterialContent final : public Aquila::UI::Core::IInspectorContent {
  public:
	MaterialContent(Aquila::SceneManagement::Entity entity, EditorContext &context);
	~MaterialContent() override;

	void build_below(Aquila::UI::Core::PropertyGrid &grid) override;
	void refresh() override;
	void on_property_edited() override;

  private:
	void show_material();
	void save_as_asset();

	Aquila::SceneManagement::Entity m_entity;
	EditorContext &m_context;
	Aquila::UI::Core::Label *m_note = nullptr;
	Aquila::UI::Core::Button *m_save_button = nullptr;
	Unique<MaterialEditorRows> m_rows;
};

}
