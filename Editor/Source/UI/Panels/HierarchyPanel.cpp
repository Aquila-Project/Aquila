#include "UI/Panels/HierarchyPanel.h"

#include "UI/Panels/HierarchyTreeNode.h"
#include "UI/Panels/HierarchyTreeView.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Scene/Components/SceneNodeComponent.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Aquila/UI/Widgets/ScrollView.h"
#include "Aquila/UI/Widgets/TextInput.h"

namespace Editor {

using namespace Aquila;
using namespace Aquila::SceneManagement;
using namespace Aquila::SceneManagement::Components;

HierarchyPanel::HierarchyPanel(EntityManager &entity_manager) : m_entity_manager(entity_manager) {}

void HierarchyPanel::build(UI::Core::View *panel, UI::Core::View *overlay_root) {
	auto ctx_uniq = std::make_unique<UI::Core::PopupMenu>();
	auto *ctx = dynamic_cast<UI::Core::PopupMenu *>(overlay_root->add_child(std::move(ctx_uniq)));
	ctx->add_item("Create Empty", [this] { on_create_requested(); });
	ctx->add_item("Create Cube", [] { AQUILA_LOG_INFO("HierarchyPanel: Create Cube (not yet implemented)"); });

	{
		auto *scroll = panel->find_by_id<UI::Core::ScrollView>("hierarchy-scroll");
		if (scroll == nullptr) {
			AQUILA_LOG_ERROR("HierarchyPanel: 'hierarchy-scroll' not found in layout");
			return;
		}

		auto tree_uniq = std::make_unique<HierarchyTreeView>(m_entity_manager);
		m_tree_view = dynamic_cast<HierarchyTreeView *>(scroll->add_child(std::move(tree_uniq)));

		populate_tree();
		populate_hierarchy_context_menu();

		m_tree_view->on_entity_selected.connect([this](Entity entity) {
			m_selected_node = m_tree_view->find_node_for_entity(entity);
			on_entity_selected(entity);
		});

		m_tree_view->on_asset_dropped.connect(
			[this](Entity entity, const UI::Core::AssetPayload &asset) { on_asset_dropped(entity, asset); });

		m_tree_view->on_deselected.connect([this] {
			AQUILA_LOG_DEBUG("Entity was deselected");
			m_selected_node = nullptr;
			on_entity_deselected();
		});

		m_tree_view->on_entity_right_clicked.connect([this](Entity entity, Vec2 pos) {
			m_selected_node = m_tree_view->find_node_for_entity(entity);
			if (m_node_context_menu != nullptr) {
				m_node_context_menu->open_at(pos);
			} 
			else {
				AQUILA_LOG_DEBUG("Node context menu is nullptr");
			}
		});

		m_tree_view->set_on_background_right_clicked([ctx](Vec2 pos) { ctx->open_at(pos); });
		scroll->on_context_menu.connect([ctx](Vec2 pos) { ctx->open_at(pos); });
		m_tree_view->on_visibility_changed.connect([this](Entity entity) { on_visibility_changed(entity); });
	}

	if (auto *filter = panel->find_by_id<UI::Core::TextInput>("hierarchy-filter")) {
		filter->set_placeholder("Filter");
		filter->on_changed.connect([this](const std::string &query) {
			if (m_tree_view != nullptr) {
				m_tree_view->apply_filter(query);
			}
		});
	}
	if (auto *create = panel->find_by_id<UI::Core::Button>("hierarchy-create")) {
		create->on_click.connect([this] { on_create_requested(); });
	}
}

void HierarchyPanel::add_entity(Entity entity) {
	if (m_tree_view != nullptr) {
		m_tree_view->add_entity_node(entity.get_name(), entity);
	}
}

void HierarchyPanel::select_entity(Entity entity) {
	if (m_tree_view == nullptr) {
		return;
	}

	HierarchyTreeNode *node = m_tree_view->find_node_for_entity(entity);
	if (node == nullptr) {
		deselect_entity();
		return;
	}

	if (node == m_selected_node) {
		return;
	}

	for (auto *scene_node = entity.try_get_component<SceneNodeComponent>();
		 scene_node != nullptr && !scene_node->parent.is_null();) {
		Entity parent = scene_node->parent;
		if (HierarchyTreeNode *parent_node = m_tree_view->find_node_for_entity(parent)) {
			parent_node->set_expanded(true);
		}
		scene_node = parent.try_get_component<SceneNodeComponent>();
	}

	m_tree_view->select_node(node);
	m_selected_node = node;
	on_entity_selected(entity);
}

void HierarchyPanel::deselect_entity() {
	if (m_tree_view == nullptr || m_selected_node == nullptr) {
		return;
	}

	m_tree_view->select_node(nullptr);
	m_selected_node = nullptr;
	on_entity_deselected();
}

void HierarchyPanel::rebuild() {
	if (m_tree_view == nullptr) {
		return;
	}
	m_tree_view->clear();
	m_selected_node = nullptr;
	m_node_context_menu = nullptr;
	populate_tree();
	populate_hierarchy_context_menu();
}

void HierarchyPanel::populate_tree() {
	m_entity_manager.for_each<MetadataComponent>([this](Entity entity) {
		auto *scene_node = entity.try_get_component<SceneNodeComponent>();
		if (scene_node == nullptr || !scene_node->parent.is_valid()) {
			m_tree_view->populate_from_entity(entity);
		}
	});
}

void HierarchyPanel::populate_hierarchy_context_menu(){
	auto node_ctx_uniq = std::make_unique<UI::Core::PopupMenu>();
	m_node_context_menu = dynamic_cast<UI::Core::PopupMenu *>(m_tree_view->add_child(std::move(node_ctx_uniq)));

	m_node_context_menu->add_item("Add child", [this] {
		if (m_selected_node != nullptr) {
			auto parent_entity = m_selected_node->get_entity();
			auto new_entity = m_entity_manager.create_entity("Empty entity");
			m_entity_manager.add_child(parent_entity, new_entity);

			m_tree_view->add_entity_node(new_entity.get_name(), new_entity, m_selected_node);
			m_selected_node->set_expanded(true);
		}
	});

	m_node_context_menu->add_item("Delete entity", [this] {
		if (m_selected_node != nullptr) {
			on_delete_requested(m_selected_node->get_entity());
		}
	});
}

void HierarchyPanel::remove_entity(Entity entity) {
	if (m_tree_view == nullptr) {
		return;
	}
	if (m_selected_node != nullptr) {
		const Entity selected = m_selected_node->get_entity();
		if (selected == entity || m_entity_manager.is_descendant(entity, selected)) {
			m_tree_view->select_node(nullptr);
			m_selected_node = nullptr;
		}
	}
	m_tree_view->delete_entity_node(entity);
	m_tree_view->queue_redraw();
}

void HierarchyPanel::sync() {
	if (m_tree_view == nullptr) {
		return;
	}

	std::vector<Entity> destroyed;
	for (const auto &[node, entity] : m_tree_view->get_nodes()) {
		if (!entity.exists()) {
			destroyed.push_back(entity);
		}
	}
	for (Entity entity : destroyed) {
		remove_entity(entity);
	}

	std::unordered_map<entt::entity, HierarchyTreeNode *> nodes;
	for (const auto &[node, entity] : m_tree_view->get_nodes()) {
		nodes[entity.get_handle()] = node;
	}

	const auto ensure_node = [this, &nodes](const auto &self, Entity entity) -> HierarchyTreeNode * {
		if (const auto it = nodes.find(entity.get_handle()); it != nodes.end()) {
			return it->second;
		}
		HierarchyTreeNode *parent = nullptr;
		if (const auto *scene_node = entity.try_get_component<SceneNodeComponent>();
			scene_node != nullptr && scene_node->parent.is_valid() &&
			scene_node->parent.has_component<MetadataComponent>()) {
			parent = self(self, scene_node->parent);
		}
		HierarchyTreeNode *node = m_tree_view->add_entity_node(entity.get_name(), entity, parent);
		nodes[entity.get_handle()] = node;
		return node;
	};

	m_entity_manager.for_each<MetadataComponent>([this, &nodes, &ensure_node](Entity entity) {
		const auto it = nodes.find(entity.get_handle());
		if (it == nodes.end()) {
			ensure_node(ensure_node, entity);
			return;
		}
		HierarchyTreeNode *node = it->second;
		if (node->get_label() != entity.get_name()) {
			node->set_label(entity.get_name());
		}
		node->refresh_visibility();
		m_tree_view->refresh_node_icon(node);
		m_tree_view->regroup(node);
	});
}

void HierarchyPanel::refresh_entity(Entity entity) {
	if (m_tree_view == nullptr) {
		return;
	}
	if (HierarchyTreeNode *node = m_tree_view->find_node_for_entity(entity)) {
		node->set_label(entity.get_name());
		m_tree_view->refresh_node_icon(node);
		m_tree_view->regroup(node);
	}
}

void HierarchyPanel::set_entity_icons(const EntityIcons &icons) {
	if (m_tree_view != nullptr) {
		m_tree_view->set_entity_icons(icons);
	}
}

void HierarchyPanel::set_tree_icons(Aquila::GFX::GfxTexture *collapsed, Aquila::GFX::GfxTexture *expanded) {
	if (m_tree_view != nullptr) {
		m_tree_view->set_expand_icons(collapsed, expanded);
	}
}

} // namespace Editor
