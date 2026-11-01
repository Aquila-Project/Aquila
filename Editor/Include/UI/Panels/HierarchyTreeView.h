#pragma once

#include "Aquila/Scene/Entity.h"
#include "Aquila/UI/Widgets/AssetPayload.h"
#include "Aquila/UI/Widgets/TreeView.h"

namespace Aquila::SceneManagement {
class EntityManager;
}

namespace Aquila::GFX {
class GfxTexture;
}

namespace Editor {

class HierarchyTreeNode;

struct EntityIcons {
	Delegate<Aquila::GFX::GfxTexture *(Aquila::SceneManagement::Entity)> entity;
	Aquila::GFX::GfxTexture *eye = nullptr;
	Aquila::GFX::GfxTexture *eye_off = nullptr;
};

class HierarchyTreeView : public Aquila::UI::Core::TreeView {
  public:
	HierarchyTreeView(Aquila::SceneManagement::EntityManager &entity_manager, std::string components_file);

	HierarchyTreeNode *add_entity_node(std::string label, Aquila::SceneManagement::Entity entity,
									   HierarchyTreeNode *parent = nullptr);
	void delete_entity_node(Aquila::SceneManagement::Entity entity);
	void populate_from_entity(Aquila::SceneManagement::Entity entity, HierarchyTreeNode *parent = nullptr);
	void clear();
	void set_entity_icons(const EntityIcons &icons);
	void refresh_node_icon(HierarchyTreeNode *node);
	void apply_filter(const std::string &query);

	[[nodiscard]] HierarchyTreeNode *find_node_for_entity(Aquila::SceneManagement::Entity entity) const;
	[[nodiscard]] const std::unordered_map<HierarchyTreeNode *, Aquila::SceneManagement::Entity> &get_nodes() const {
		return m_node_entity_map;
	}
	[[nodiscard]] Aquila::SceneManagement::EntityManager &get_entity_manager() { return m_entity_manager; }
	[[nodiscard]] const EntityIcons &get_icons() const { return m_icons; }

	Signal<void(Aquila::SceneManagement::Entity)> on_entity_selected;
	Signal<void(Aquila::SceneManagement::Entity, Vec2)> on_entity_right_clicked;
	Signal<void(Aquila::SceneManagement::Entity)> on_visibility_changed;
	Signal<void(Aquila::SceneManagement::Entity, const Aquila::UI::Core::AssetPayload &)> on_asset_dropped;

  private:
	void on_drop(Aquila::UI::Core::DragState &state) override;
	bool filter_node(Aquila::UI::Core::TreeNode *node, const std::string &query);

	[[nodiscard]] Aquila::GFX::GfxTexture *icon_for(Aquila::SceneManagement::Entity entity) const;

	EntityIcons m_icons;
	Aquila::SceneManagement::EntityManager &m_entity_manager;
	std::string m_components_file;
	std::unordered_map<HierarchyTreeNode *, Aquila::SceneManagement::Entity> m_node_entity_map;
	std::string m_filter;
};

} // namespace Editor
