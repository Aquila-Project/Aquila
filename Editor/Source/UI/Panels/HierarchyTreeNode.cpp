#include "UI/Panels/HierarchyTreeNode.h"

#include "UI/Panels/HierarchyTreeView.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/UI/Widgets/Button.h"

namespace Editor {

using namespace Aquila::SceneManagement;
using namespace Aquila::UI::Core;

HierarchyTreeNode::HierarchyTreeNode(std::string label, HierarchyTreeView &owner, int depth, Entity entity,
									 EntityManager &entity_manager)
	: TreeNode(std::move(label), owner, depth), m_entity(entity), m_entity_manager(entity_manager),
	  m_hierarchy_view(owner) {
	m_is_draggable = true;
	m_is_accepting_payload = true;

	m_eye = get_actions()->add_child<Button>();
	m_eye->add_class("hierarchy-eye");
	m_eye->on_click.connect([this] { toggle_visibility(); });
	track_row_hover(m_eye);
	on_row_hover_changed.connect([this](bool hovered) {
		m_row_hovered = hovered;
		refresh_visibility();
	});
}

void HierarchyTreeNode::toggle_visibility() {
	auto *metadata = m_entity.try_get_component<Components::MetadataComponent>();
	if (metadata == nullptr) {
		return;
	}
	metadata->set_visible(!metadata->is_visible());
	refresh_visibility();
	m_hierarchy_view.on_visibility_changed(m_entity);
}

void HierarchyTreeNode::refresh_visibility() {
	const auto *metadata = m_entity.try_get_component<Components::MetadataComponent>();
	const bool visible = metadata == nullptr || metadata->is_visible();
	const EntityIcons &icons = m_hierarchy_view.get_icons();
	const bool shown = m_row_hovered || !visible;
	m_eye->set_icon(shown ? (visible ? icons.eye : icons.eye_off) : nullptr);
	m_eye->set_tooltip(visible ? "Hide" : "Show");
	m_eye->set_class("hierarchy-eye-shown", shown);
	get_header()->set_class("hierarchy-entity-hidden", !visible);
}

void HierarchyTreeNode::on_drag_start(DragState &state) {
	state.payload = m_entity;
	state.label = get_label();
}

void HierarchyTreeNode::on_drop(DragState &state) {
	const auto *dragged = std::any_cast<Entity>(&state.payload);
	if (dragged == nullptr) {
		return;
	}
	Entity dragged_entity = *dragged;
	if (dragged_entity == m_entity) {
		return;
	}

	HierarchyTreeNode *source_node = m_hierarchy_view.find_node_for_entity(dragged_entity);
	if (source_node == nullptr || source_node == this) {
		return;
	}

	m_entity_manager.add_child(m_entity, dragged_entity);

	View *old_parent_container = source_node->get_parent();
	auto *old_parent_node = view_cast<TreeNode>((old_parent_container != nullptr) ? old_parent_container->get_parent() : nullptr);

	auto detached = old_parent_container->detach_child(source_node);

	if (old_parent_node != nullptr) {
		old_parent_node->refresh_indicator();
		old_parent_node->queue_redraw();
	}

	auto *new_node = dynamic_cast<HierarchyTreeNode *>(add_child(std::move(detached)));
	if (new_node != nullptr) {
		new_node->update_depth(get_depth() + 1);
		set_expanded(true);
	}
	m_hierarchy_view.refresh_groups();
}

} // namespace Editor
