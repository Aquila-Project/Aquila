#include "UI/Panels/HierarchyTreeView.h"

#include "UI/Panels/HierarchyTreeNode.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/Scene/Components/SceneNodeComponent.h"
#include "Aquila/Scene/EntityManager.h"

namespace Editor {

using namespace Aquila::SceneManagement;
using namespace Aquila::SceneManagement::Components;
using namespace Aquila::UI::Core;

HierarchyTreeView::HierarchyTreeView(EntityManager &entity_manager, std::string components_file)
	: m_entity_manager(entity_manager), m_components_file(std::move(components_file)) {
	set_row_component(m_components_file, "HierarchyRow");
	m_is_accepting_payload = true;
	m_is_draggable = false;

	on_selected.connect([this](Aquila::UI::Core::TreeNode *node) {
		auto *hierarchy_node = dynamic_cast<HierarchyTreeNode *>(node);
		auto it = m_node_entity_map.find(hierarchy_node);
		if (it != m_node_entity_map.end()) {
			on_entity_selected(it->second);
		}
	});

	on_node_right_clicked.connect([this](Aquila::UI::Core::TreeNode *node, Vec2 pos) {
		auto *hierarchy_node = dynamic_cast<HierarchyTreeNode *>(node);
		auto it = m_node_entity_map.find(hierarchy_node);
		if (it != m_node_entity_map.end()) {
			on_entity_right_clicked(it->second, pos);
		}
	});
}

HierarchyTreeNode *HierarchyTreeView::add_entity_node(std::string label, Entity entity, HierarchyTreeNode *parent) {
	const int depth = parent != nullptr ? parent->get_depth() + 1 : 0;
	auto node = std::make_unique<HierarchyTreeNode>(std::move(label), *this, depth, entity, m_entity_manager);

	auto *raw = parent != nullptr ? parent->add_child(std::move(node)) : add_child(std::move(node));
	m_node_entity_map[raw] = entity;
	raw->set_type_icon(icon_for(entity));
	raw->refresh_visibility();
	if (!m_filter.empty()) {
		apply_filter(m_filter);
	}
	return raw;
}

Aquila::GFX::GfxTexture *HierarchyTreeView::icon_for(Entity entity) const {
	return m_icons.entity ? m_icons.entity(entity) : nullptr;
}

void HierarchyTreeView::set_entity_icons(const EntityIcons &icons) {
	m_icons = icons;
	for (const auto &[node, entity] : m_node_entity_map) {
		node->set_type_icon(icon_for(entity));
		node->refresh_visibility();
	}
}

void HierarchyTreeView::refresh_node_icon(HierarchyTreeNode *node) {
	const auto it = m_node_entity_map.find(node);
	if (it != m_node_entity_map.end()) {
		node->set_type_icon(icon_for(it->second));
	}
}

void HierarchyTreeView::delete_entity_node(Entity entity) {
	HierarchyTreeNode *target = find_node_for_entity(entity);
	if (target == nullptr) {
		return;
	}
	std::erase_if(m_node_entity_map, [target](const auto &entry) {
		for (const View *view = entry.first; view != nullptr; view = view->get_parent()) {
			if (view == target) {
				return true;
			}
		}
		return false;
	});
	remove_node(target);
}

void HierarchyTreeView::populate_from_entity(Entity entity, HierarchyTreeNode *parent) {
	auto *node = add_entity_node(entity.get_name(), entity, parent);
	if (auto *scene_node = entity.try_get_component<SceneNodeComponent>()) {
		for (auto child : scene_node->children) {
			populate_from_entity(child, node);
		}
	}
}

void HierarchyTreeView::clear() {
	deselect();
	if (m_content != nullptr) {
		m_content->clear_children();
	}
	m_node_entity_map.clear();
}

HierarchyTreeNode *HierarchyTreeView::find_node_for_entity(Entity entity) const {
	for (const auto &[node, e] : m_node_entity_map) {
		if (e == entity) {
			return node;
		}
	}
	return nullptr;
}

bool HierarchyTreeView::filter_node(TreeNode *node, const std::string &query) {
	bool any_child = false;
	for (const auto &child : node->get_children_container()->get_children()) {
		if (auto *child_node = view_cast<TreeNode>(child.get())) {
			any_child = filter_node(child_node, query) || any_child;
		}
	}
	const bool self = query.empty() || Aquila::Foundation::to_lower(node->get_label()).find(query) != std::string::npos;
	const bool shown = self || any_child;
	node->set_hidden(!shown);
	return shown;
}

void HierarchyTreeView::apply_filter(const std::string &query) {
	m_filter = Aquila::Foundation::to_lower(query);
	for (const auto &child : m_content->get_children()) {
		if (auto *node = view_cast<TreeNode>(child.get())) {
			filter_node(node, m_filter);
		}
	}
}

void HierarchyTreeView::on_drop(DragState &state) {
	const auto *dragged = std::any_cast<Entity>(&state.payload);
	if (dragged == nullptr) {
		return;
	}

	Entity dragged_entity = *dragged;
	HierarchyTreeNode *source_node = find_node_for_entity(dragged_entity);
	if (source_node == nullptr) {
		return;
	}

	View *container = source_node->get_parent();
	if (container == nullptr || container == m_content) {
		return;
	}

	auto *node = dragged_entity.try_get_component<Components::SceneNodeComponent>();
	if (node != nullptr && node->parent.is_valid()) {
		m_entity_manager.remove_child(node->parent, dragged_entity);
	}

	auto *old_owner = view_cast<TreeNode>(container->get_parent());
	Unique<View> detached = container->detach_child(source_node);
	if (old_owner != nullptr) {
		old_owner->refresh_indicator();
	}
	if (auto *moved = dynamic_cast<HierarchyTreeNode *>(add_child(std::move(detached)))) {
		moved->update_depth(0);
		moved->refresh_subtree_visibility();
	}
}

} // namespace Editor
