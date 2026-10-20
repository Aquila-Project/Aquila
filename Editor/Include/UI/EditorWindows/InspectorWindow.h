#pragma once

#include "Core/EditorWindow.h"

namespace Aquila::UI::Core {
class Image;
class Label;
class PropertyGrid;
class View;
}

namespace Editor {

class InspectorPanel;
class MaterialEditorRows;

class InspectorWindow final : public EditorWindow {
  public:
	explicit InspectorWindow(EditorContext &context);
	~InspectorWindow() override;

	void build(Aquila::UI::Core::View &content) override;
	void open_add_search(Vec2 canvas_pos);

  private:
	void show(Aquila::SceneManagement::Entity entity);
	/// A .aqmat picked in the content browser: its editor replaces the entity view until an entity is selected.
	void show_asset(const std::string &path);
	void show_preview(const std::string &path);
	void build_asset_view(Aquila::UI::Core::View &content);

	Unique<InspectorPanel> m_panel;
	Aquila::UI::Core::View *m_entity_root = nullptr;
	Aquila::UI::Core::View *m_asset_view = nullptr;
	Aquila::UI::Core::Image *m_asset_preview = nullptr;
	Aquila::UI::Core::Label *m_asset_title = nullptr;
	Aquila::UI::Core::Label *m_asset_path = nullptr;
	Unique<MaterialEditorRows> m_asset_rows;
	std::string m_asset;
};

}
