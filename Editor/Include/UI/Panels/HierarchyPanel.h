#pragma once

#include "Aquila/UI/Widgets/AssetPayload.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "UI/Panels/IEditorPanel.h"
#include "Aquila/Scene/Entity.h"

namespace Aquila::SceneManagement {
class EntityManager;
}

namespace Aquila::GFX {
class GfxTexture;
}

namespace Editor {

class HierarchyTreeView;
class HierarchyTreeNode;
struct EntityIcons;

class HierarchyPanel : public IEditorPanel {
  public:
	explicit HierarchyPanel(Aquila::SceneManagement::EntityManager &entity_manager);
	void build(Aquila::UI::Core::View *panel, Aquila::UI::Core::View *overlay_root) override;
	Signal<void(Aquila::SceneManagement::Entity)> on_entity_selected;
	Signal<void()> on_entity_deselected;
	Signal<void()> on_create_requested;
	Signal<void(Aquila::SceneManagement::Entity)> on_delete_requested;
	Signal<void(Aquila::SceneManagement::Entity)> on_visibility_changed;
	Signal<void(Aquila::SceneManagement::Entity, const Aquila::UI::Core::AssetPayload &)> on_asset_dropped;
	void add_entity(Aquila::SceneManagement::Entity entity);
	void select_entity(Aquila::SceneManagement::Entity entity);
	void deselect_entity();
	void refresh_entity(Aquila::SceneManagement::Entity entity);
	void remove_entity(Aquila::SceneManagement::Entity entity);
	void rebuild();
	void set_tree_icons(Aquila::GFX::GfxTexture *collapsed, Aquila::GFX::GfxTexture *expanded);
	void set_entity_icons(const EntityIcons &icons);

  private:
	void populate_tree();
	void populate_hierarchy_context_menu();

	Aquila::SceneManagement::EntityManager &m_entity_manager;
	HierarchyTreeView *m_tree_view = nullptr;
	HierarchyTreeNode *m_selected_node = nullptr;
	Aquila::UI::Core::PopupMenu* m_node_context_menu = nullptr;
};

} // namespace Editor
