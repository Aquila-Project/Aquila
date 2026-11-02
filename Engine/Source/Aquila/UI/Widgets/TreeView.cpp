#include "Aquila/UI/Widgets/TreeView.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/MarkupItem.h"

namespace Aquila::UI::Core {

namespace {

void add_markup_nodes(TreeNode &parent, const std::vector<MarkupItem> &items) {
	for (const MarkupItem &item : items) {
		add_markup_nodes(*parent.add_child_node(std::string(item.get("label"))), item.items);
	}
}

}

TreeView::TreeView() {
	add_class("tree-view");

	auto content = std::make_unique<View>();
	content->add_class("tree-view-content");
	m_content = View::add_child(std::move(content));

	m_content->on_pressed.connect([this](Vec2) { deselect(); });
	on_pressed.connect([this](Vec2) { deselect(); });
}

TreeNode *TreeView::add_node(std::string label) {
	auto node = std::make_unique<TreeNode>(std::move(label), *this, 0);
	return add_child(std::move(node));
}

View *TreeView::add_child(Unique<View> child) {
	View *added = m_content->View::add_child(std::move(child));
	if (auto *node = view_cast<TreeNode>(added)) {
		node->build_row_components();
	}
	return added;
}

bool TreeView::add_markup_item(MarkupItem &item, IResourceResolver *) {
	if (item.tag != "TreeNode") {
		return false;
	}
	add_markup_nodes(*add_node(std::string(item.get("label"))), item.items);
	return true;
}

void TreeView::set_row_component(std::string file, std::string component) {
	m_row_file = std::move(file);
	m_row_component = std::move(component);
}

void TreeView::on_drag_enter(DragState &) {
	add_class("drag-target");
}

void TreeView::on_drag_leave(DragState &) {
	remove_class("drag-target");
}

static bool is_descendant_of(View *candidate, View *ancestor) {
	while (candidate != nullptr) {
		if (candidate == ancestor) {
			return true;
		}
		candidate = candidate->get_parent();
	}
	return false;
}

void TreeView::remove_node(TreeNode *node) {
	if (node == nullptr) {
		return;
	}

	if (node->get_parent() == nullptr) {
		return;
	}

	if (m_selected != nullptr && is_descendant_of(m_selected, node)) {
		m_selected = nullptr;
	}

	View *container = node->get_parent();
	container->remove_child(node);
	if (auto *owner_node = view_cast<TreeNode>(container->get_parent())) {
		owner_node->refresh_indicator();
	}
}

void TreeView::set_on_background_right_clicked(Delegate<void(Vec2)> callback) {
	m_content->on_context_menu.set(std::move(callback));
}

void TreeView::clear() {
	m_selected = nullptr;
	m_content->clear_children();
}

void TreeView::select_node(TreeNode *node) {
	if (m_selected != nullptr) {
		m_selected->set_selected(false);
	}
	m_selected = node;
	if (m_selected != nullptr) {
		m_selected->set_selected(true);
	}
}

void TreeView::deselect() {
	if (m_selected == nullptr) {
		AQUILA_LOG_DEBUG("Selected is null already");
		return;
	}
	select_node(nullptr);
	on_deselected();
}

void TreeView::notify_selected(TreeNode *node) {
	select_node(node);
	on_selected(node);
}

void TreeView::notify_right_clicked(TreeNode *node, Vec2 pos) {
	on_node_right_clicked(node, pos);
}

TreeNode::TreeNode(std::string label, TreeView &owner, int depth)
	: m_owner(owner), m_label(std::move(label)), m_depth(depth) {
	add_class("tree-node");

	auto row = std::make_unique<View>();
	row->add_class("tree-node-row");
	m_row = View::add_child(std::move(row));

	auto header = std::make_unique<Button>();
	header->add_class("tree-node-header");
	header->on_click.connect([this] { on_header_clicked(); });
	header->on_context_menu.connect([this](Vec2 pos) { on_header_right_clicked(pos); });
	m_header = m_row->add_child(std::move(header));

	auto actions = std::make_unique<View>();
	actions->add_class("tree-node-actions");
	m_actions = m_row->add_child(std::move(actions));

	track_row_hover(m_header);

	auto children = std::make_unique<View>();
	children->add_class("tree-node-children");
	m_children = View::add_child(std::move(children));
	update_header_text();
}

View *TreeNode::add_child(Unique<View> node) {
	auto *added = m_children->View::add_child(std::move(node));
	update_header_text();
	queue_redraw();
	if (auto *child = view_cast<TreeNode>(added)) {
		child->build_row_components();
	}
	return added;
}

void TreeNode::set_row_component(std::string file, std::string component) {
	m_row_file = std::move(file);
	m_row_component = std::move(component);
}

void TreeNode::build_row_components() {
	if (LayoutLoader *loader = find_layout_loader(*this)) {
		build_rows(*loader);
		return;
	}
	const bool has_row = !m_row_component.empty() || !m_owner.m_row_component.empty();
	if (has_row && find_layout_loader(m_owner) == nullptr) {
		AQUILA_LOG_ERROR("TreeView: '{}' needs a canvas with a layout loader for its row", m_label);
	}
}

void TreeNode::build_rows(LayoutLoader &loader) {
	const bool own = !m_row_component.empty();
	const std::string &component = own ? m_row_component : m_owner.m_row_component;
	if (m_row_view == nullptr && !component.empty()) {
		m_row_view = loader.create(own ? m_row_file : m_owner.m_row_file, component, *m_actions);
		if (m_row_view != nullptr) {
			on_row_component_built();
		}
	}
	for (const auto &child : m_children->get_children()) {
		if (auto *node = view_cast<TreeNode>(child.get())) {
			node->build_rows(loader);
		}
	}
}

void TreeNode::on_drag_enter(DragState &) {
	m_header->add_class("drag-target");
}

void TreeNode::on_drag_leave(DragState &) {
	m_header->remove_class("drag-target");
}

TreeNode *TreeNode::add_child_node(std::string label) {
	auto node = std::make_unique<TreeNode>(std::move(label), m_owner, m_depth + 1);
	return add_child(std::move(node));
}

void TreeNode::update_depth(int new_depth) {
	m_depth = new_depth;

	for (const auto &child : m_children->get_children()) {
		if (auto *child_node = view_cast<TreeNode>(child.get())) {
			child_node->update_depth(m_depth + 1);
		}
	}
}

void TreeNode::set_label(std::string label) {
	m_label = std::move(label);
	update_header_text();
}

void TreeNode::set_expanded(bool expanded) {
	if (expanded == m_expanded) {
		return;
	}
	m_expanded = expanded;
	apply_state();
	update_header_text();
	queue_redraw();
}

void TreeNode::set_selected(bool selected) {
	m_header->set_class("tree-node-selected", selected);
	m_row->set_class("tree-node-row-selected", selected);
}

void TreeNode::track_row_hover(View *view) {
	if (view == nullptr) {
		return;
	}
	view->on_mouse_entered.connect([this] {
		if (m_row_hover_count++ == 0) {
			m_row->add_class("tree-node-row-hover");
			on_row_hover_changed(true);
		}
	});
	view->on_mouse_left.connect([this] {
		if (m_row_hover_count > 0 && --m_row_hover_count == 0) {
			m_row->remove_class("tree-node-row-hover");
			on_row_hover_changed(false);
		}
	});
}

void TreeNode::apply_state() {
	m_children->set_hidden(!m_expanded);
}

void TreeNode::on_header_clicked() {
	set_expanded(!m_expanded);
	if (m_selectable) {
		m_owner.notify_selected(this);
	}
}

void TreeNode::on_header_right_clicked(Vec2 pos) {
	m_owner.notify_right_clicked(this, pos);
}

void TreeNode::set_type_icon(GFX::GfxTexture *icon) {
	m_type_icon = icon;
	update_header_text();
}

void TreeNode::update_header_text() {
	if ((m_header == nullptr) || (m_children == nullptr)) {
		return;
	}
	const bool has_children = !m_children->get_children().empty();
	const bool use_icons = (m_owner.m_icon_collapsed != nullptr) || (m_owner.m_icon_expanded != nullptr);

	if (m_type_icon != nullptr) {
		m_header->set_reserve_icon_space(true);
		m_header->set_icon(m_type_icon);
		m_header->set_trailing_icon(has_children ? (m_expanded ? m_owner.m_icon_expanded : m_owner.m_icon_collapsed)
												 : nullptr);
		m_header->set_text(m_label);
		return;
	}
	m_header->set_trailing_icon(nullptr);

	if (use_icons) {
		m_header->set_reserve_icon_space(true);
		m_header->set_icon(has_children ? (m_expanded ? m_owner.m_icon_expanded : m_owner.m_icon_collapsed) : nullptr);
		m_header->set_text(m_label);
		return;
	}

	m_header->set_reserve_icon_space(false);
	m_header->set_icon(nullptr);
	if (has_children) {
		m_header->set_text(std::string(m_expanded ? "v " : "> ") + m_label);
	} else {
		m_header->set_text(m_label);
	}
}

void TreeNode::refresh_indicator() {
	update_header_text();
}

void TreeView::set_expand_icons(GFX::GfxTexture *collapsed, GFX::GfxTexture *expanded) {
	m_icon_collapsed = collapsed;
	m_icon_expanded = expanded;
	if (m_content != nullptr) {
		refresh_indicators(m_content);
	}
}

void TreeView::refresh_indicators(View *node) {
	for (const auto &child : node->get_children()) {
		if (auto *tree_node = view_cast<TreeNode>(child.get())) {
			tree_node->refresh_indicator();
		}
		refresh_indicators(child.get());
	}
}

} // namespace Aquila::UI::Core
