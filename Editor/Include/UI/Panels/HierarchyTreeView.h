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

namespace Aquila::UI::Core {
class Label;
}

namespace Editor {

class HierarchyTreeNode;

struct EntityIcons {
	Aquila::GFX::GfxTexture *empty = nullptr;
	Aquila::GFX::GfxTexture *mesh = nullptr;
	Aquila::GFX::GfxTexture *light = nullptr;
	Aquila::GFX::GfxTexture *camera = nullptr;
	Aquila::GFX::GfxTexture *sky = nullptr;
	Aquila::GFX::GfxTexture *eye = nullptr;
	Aquila::GFX::GfxTexture *eye_off = nullptr;
};

enum class EntityGroup { Environment, Lights, Geometry, Cameras, Other, Count };

class HierarchyTreeView : public Aquila::UI::Core::TreeView {
  public:
	explicit HierarchyTreeView(Aquila::SceneManagement::EntityManager &entity_manager);

	HierarchyTreeNode *add_entity_node(std::string label, Aquila::SceneManagement::Entity entity,
									   HierarchyTreeNode *parent = nullptr);
	void delete_entity_node(Aquila::SceneManagement::Entity entity);
	void populate_from_entity(Aquila::SceneManagement::Entity entity, HierarchyTreeNode *parent = nullptr);
	void clear();
	void set_entity_icons(const EntityIcons &icons);
	void refresh_node_icon(HierarchyTreeNode *node);
	void regroup(HierarchyTreeNode *node);
	void refresh_groups();
	void apply_filter(const std::string &query);
	void move_to_group(HierarchyTreeNode *node);

	[[nodiscard]] HierarchyTreeNode *find_node_for_entity(Aquila::SceneManagement::Entity entity) const;
	[[nodiscard]] Aquila::SceneManagement::EntityManager &get_entity_manager() { return m_entity_manager; }
	[[nodiscard]] const EntityIcons &get_icons() const { return m_icons; }
	/// The group an entity is listed under.
	[[nodiscard]] static EntityGroup group_for(Aquila::SceneManagement::Entity entity);

	Signal<void(Aquila::SceneManagement::Entity)> on_entity_selected;
	Signal<void(Aquila::SceneManagement::Entity, Vec2)> on_entity_right_clicked;
	Signal<void(Aquila::SceneManagement::Entity)> on_visibility_changed;
	Signal<void(Aquila::SceneManagement::Entity, const Aquila::UI::Core::AssetPayload &)> on_asset_dropped;

  private:
	struct Group {
		Aquila::UI::Core::TreeNode *node = nullptr;
		Aquila::UI::Core::Label *count = nullptr;
	};

	void on_drop(Aquila::UI::Core::DragState &state) override;
	Aquila::UI::Core::TreeNode *ensure_group(EntityGroup group);
	[[nodiscard]] bool is_group_node(const Aquila::UI::Core::View *view) const;
	bool filter_node(Aquila::UI::Core::TreeNode *node, const std::string &query);

	[[nodiscard]] Aquila::GFX::GfxTexture *icon_for(Aquila::SceneManagement::Entity entity) const;

	EntityIcons m_icons;
	Aquila::SceneManagement::EntityManager &m_entity_manager;
	std::unordered_map<HierarchyTreeNode *, Aquila::SceneManagement::Entity> m_node_entity_map;
	std::array<Group, static_cast<Usize>(EntityGroup::Count)> m_groups{};
	std::string m_filter;
};

} // namespace Editor
