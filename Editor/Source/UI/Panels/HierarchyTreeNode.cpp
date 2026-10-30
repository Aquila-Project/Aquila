#include "UI/Panels/HierarchyTreeNode.h"

#include "UI/Components/HierarchyRow.h"
#include "UI/Panels/HierarchyTreeView.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/UI/Widgets/AssetPayload.h"

namespace Editor {

using namespace Aquila::SceneManagement;
using namespace Aquila::UI::Core;

HierarchyTreeNode::HierarchyTreeNode(std::string label, HierarchyTreeView &owner, int depth, Entity entity,
									 EntityManager &entity_manager)
	: TreeNode(std::move(label), owner, depth), m_entity(entity), m_entity_manager(entity_manager),
	  m_hierarchy_view(owner) {
	m_is_draggable = true;
	m_is_accepting_payload = true;

	on_row_hover_changed.connect([this](bool hovered) {
		m_row_hovered = hovered;
		refresh_visibility();
	});
}

void HierarchyTreeNode::on_row_component_built() {
	auto *row = get_row_behaviour<HierarchyRow>();
	if (row == nullptr) {
		return;
	}
	row->on_toggle_visibility.connect([this] { toggle_visibility(); });
	track_row_hover(row->get_eye());
	refresh_visibility();
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
	if (auto *row = get_row_behaviour<HierarchyRow>()) {
		row->set_visible(visible, m_row_hovered, m_hierarchy_view.get_icons());
	}
	get_header()->set_class("hierarchy-entity-hidden", !visible);
}

void HierarchyTreeNode::on_drag_start(DragState &state) {
	state.payload = m_entity;
	state.label = get_label();
}

void HierarchyTreeNode::on_drag_enter(DragState &state) {
	TreeNode::on_drag_enter(state);
	const auto *asset = std::any_cast<AssetPayload>(&state.payload);
	const bool accepted =
		std::any_cast<Entity>(&state.payload) != nullptr || (asset != nullptr && asset->asset_type == "script");
	get_row()->set_class("hierarchy-drop-target", accepted);
}

void HierarchyTreeNode::on_drag_leave(DragState &state) {
	TreeNode::on_drag_leave(state);
	get_row()->remove_class("hierarchy-drop-target");
}

void HierarchyTreeNode::on_drop(DragState &state) {
	if (const auto *asset = std::any_cast<AssetPayload>(&state.payload)) {
		m_hierarchy_view.on_asset_dropped(m_entity, *asset);
		return;
	}

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
	auto *old_parent_node =
		view_cast<TreeNode>((old_parent_container != nullptr) ? old_parent_container->get_parent() : nullptr);

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
