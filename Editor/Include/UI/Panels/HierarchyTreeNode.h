#pragma once

#include "Aquila/Scene/Entity.h"
#include "Aquila/UI/Widgets/TreeView.h"

namespace Aquila::SceneManagement {
class EntityManager;
}

namespace Editor {

class HierarchyTreeView;

class HierarchyTreeNode : public Aquila::UI::Core::TreeNode {
  public:
	HierarchyTreeNode(std::string label, HierarchyTreeView &owner, int depth,
	                  Aquila::SceneManagement::Entity entity,
	                  Aquila::SceneManagement::EntityManager &entity_manager);

	[[nodiscard]] Aquila::SceneManagement::Entity get_entity() const { return m_entity; }
	void refresh_visibility();

  private:
	void on_drag_start(Aquila::UI::Core::DragState &state) override;
	void on_drop(Aquila::UI::Core::DragState &state) override;
	void on_drag_enter(Aquila::UI::Core::DragState &state) override;
	void on_drag_leave(Aquila::UI::Core::DragState &state) override;

	void toggle_visibility();

	Aquila::UI::Core::Button *m_eye = nullptr;
	bool m_row_hovered = false;
	Aquila::SceneManagement::Entity m_entity;
	Aquila::SceneManagement::EntityManager &m_entity_manager;
	HierarchyTreeView &m_hierarchy_view;
};

} // namespace Editor
