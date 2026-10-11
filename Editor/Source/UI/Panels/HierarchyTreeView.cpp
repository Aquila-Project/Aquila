#include "UI/Panels/HierarchyTreeView.h"

#include "UI/Panels/HierarchyTreeNode.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Components/LightComponent.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/SceneNodeComponent.h"
#include "Aquila/Scene/Components/SkyLightComponent.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/UI/Widgets/Label.h"

#include <algorithm>
#include <cctype>

namespace Editor {

using namespace Aquila::SceneManagement;
using namespace Aquila::SceneManagement::Components;
using namespace Aquila::UI::Core;

namespace {

constexpr std::array<const char *, static_cast<Usize>(EntityGroup::Count)> k_group_labels = {
	"ENVIRONMENT", "LIGHTS", "GEOMETRY", "CAMERAS", "EMPTY",
};

}

HierarchyTreeView::HierarchyTreeView(EntityManager &entity_manager) : m_entity_manager(entity_manager) {
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

EntityGroup HierarchyTreeView::group_for(Entity entity) {
	if (entity.has_component<CameraComponent>()) {
		return EntityGroup::Cameras;
	}
	if (entity.has_component<SkyLightComponent>()) {
		return EntityGroup::Environment;
	}
	if (entity.has_component<LightComponent>()) {
		return EntityGroup::Lights;
	}
	if (entity.has_component<MeshComponent>()) {
		return EntityGroup::Geometry;
	}
	return EntityGroup::Other;
}

bool HierarchyTreeView::is_group_node(const View *view) const {
	return std::ranges::any_of(m_groups, [view](const Group &group) { return group.node != nullptr && group.node == view; });
}

TreeNode *HierarchyTreeView::ensure_group(EntityGroup group) {
	Group &slot = m_groups[static_cast<Usize>(group)];
	if (slot.node != nullptr) {
		return slot.node;
	}

	auto node = std::make_unique<TreeNode>(k_group_labels[static_cast<Usize>(group)], *this, 0);
	node->set_selectable(false);
	node->get_header()->add_class("hierarchy-group-header");
	slot.count = node->get_actions()->add_child<Label>(std::string{});
	slot.count->add_class("hierarchy-group-count");
	slot.node = dynamic_cast<TreeNode *>(add_child(std::move(node)));

	for (Usize next = static_cast<Usize>(group) + 1; next < m_groups.size(); ++next) {
		if (m_groups[next].node != nullptr) {
			m_content->reorder_child(slot.node, m_groups[next].node);
			break;
		}
	}
	return slot.node;
}

HierarchyTreeNode *HierarchyTreeView::add_entity_node(std::string label, Entity entity, HierarchyTreeNode *parent) {
	TreeNode *container = parent != nullptr ? static_cast<TreeNode *>(parent) : ensure_group(group_for(entity));
	const int depth = container->get_depth() + 1;
	auto node = std::make_unique<HierarchyTreeNode>(std::move(label), *this, depth, entity, m_entity_manager);

	auto *raw = dynamic_cast<HierarchyTreeNode *>(container->add_child(std::move(node)));
	m_node_entity_map[raw] = entity;
	raw->set_type_icon(icon_for(entity));
	raw->refresh_visibility();
	refresh_groups();
	if (!m_filter.empty()) {
		apply_filter(m_filter);
	}
	return raw;
}

Aquila::GFX::GfxTexture *HierarchyTreeView::icon_for(Entity entity) const {
	if (entity.has_component<CameraComponent>()) {
		return m_icons.camera;
	}
	if (entity.has_component<SkyLightComponent>()) {
		return m_icons.sky;
	}
	if (entity.has_component<LightComponent>()) {
		return m_icons.light;
	}
	if (entity.has_component<MeshComponent>()) {
		return m_icons.mesh;
	}
	return m_icons.empty;
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

void HierarchyTreeView::regroup(HierarchyTreeNode *node) {
	if (node == nullptr) {
		return;
	}
	View *container = node->get_parent();
	View *owner = container != nullptr ? container->get_parent() : nullptr;
	if (!is_group_node(owner)) {
		return;
	}
	const TreeNode *target = m_groups[static_cast<Usize>(group_for(node->get_entity()))].node;
	if (owner != target) {
		move_to_group(node);
	}
}

void HierarchyTreeView::move_to_group(HierarchyTreeNode *node) {
	View *container = node->get_parent();
	if (container == nullptr) {
		return;
	}
	auto *old_owner = view_cast<TreeNode>(container->get_parent());
	Unique<View> detached = container->detach_child(node);
	if (old_owner != nullptr) {
		old_owner->refresh_indicator();
	}

	TreeNode *group = ensure_group(group_for(node->get_entity()));
	auto *moved = dynamic_cast<HierarchyTreeNode *>(group->add_child(std::move(detached)));
	if (moved != nullptr) {
		moved->update_depth(group->get_depth() + 1);
		group->set_expanded(true);
	}
	refresh_groups();
}

void HierarchyTreeView::refresh_groups() {
	for (Group &group : m_groups) {
		if (group.node == nullptr) {
			continue;
		}
		Usize count = 0;
		for (const auto &child : group.node->get_children_container()->get_children()) {
			if (view_is<TreeNode>(child.get())) {
				++count;
			}
		}
		if (count == 0) {
			remove_node(group.node);
			group = Group{};
			continue;
		}
		group.count->set_text(std::to_string(count));
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
	refresh_groups();
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
		while (!m_content->get_children().empty()) {
			m_content->remove_child(m_content->get_children().front().get());
		}
	}
	m_node_entity_map.clear();
	m_groups = {};
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
	for (Group &group : m_groups) {
		if (group.node == nullptr) {
			continue;
		}
		bool any = false;
		for (const auto &child : group.node->get_children_container()->get_children()) {
			if (auto *child_node = view_cast<TreeNode>(child.get())) {
				any = filter_node(child_node, m_filter) || any;
			}
		}
		group.node->set_hidden(!any);
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
	if (container != nullptr && is_group_node(container->get_parent())) {
		return;
	}

	auto *node = dragged_entity.try_get_component<Components::SceneNodeComponent>();
	if (node != nullptr && node->parent.is_valid()) {
		m_entity_manager.remove_child(node->parent, dragged_entity);
	}
	move_to_group(source_node);
}

} // namespace Editor
