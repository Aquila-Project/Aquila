#include "Aquila/UI/DevTools/UIDebugPanel.h"

#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Style/StyleProperties.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/ScrollView.h"
#include "Aquila/UI/Widgets/TreeView.h"

namespace Aquila::UI::DevTools {

using namespace Aquila::UI::Core;

namespace {

std::string make_label(View *view) {
	std::string label(view->get_type_name());
	if (!view->get_id().empty()) {
		label += " #" + view->get_id();
	}
	for (const auto &cls : view->get_classes()) {
		label += " ." + cls;
	}
	return label;
}

} // namespace

void UIDebugPanel::build(View *overlay_root, Canvas *target) {
	m_target = target;

	m_window = overlay_root->add_child<View>();
	m_window->set_id("ui-debug-window");
	m_window->add_class("ui-debug-window");

	UI::FloatingConfig floating;
	floating.attach_to = UI::FloatingAttachTo::Root;
	floating.parent_point = UI::FloatingAttachPoint::LeftTop;
	floating.element_point = UI::FloatingAttachPoint::LeftTop;
	floating.offset = { 60.F, 60.F };
	floating.z_index = 1000;
	m_window->set_floating(floating);

	auto *header = m_window->add_child<View>();
	header->add_class("ui-debug-header");

	auto *title = header->add_child<Label>(std::string("UI Inspector"));
	title->add_class("ui-debug-title");

	auto *refresh = header->add_child<Button>(std::string("Refresh"));
	refresh->add_class("ui-debug-refresh");
	refresh->on_click.connect([this] { this->refresh(); });

	auto *scroll = m_window->add_child<ScrollView>();
	scroll->add_class("ui-debug-body");
	m_tree_host = scroll->add_child<View>();
	m_tree_host->add_class("ui-debug-tree-host");

	m_window->set_hidden(true);
}

void UIDebugPanel::toggle() {
	if (m_window == nullptr) {
		return;
	}

	m_visible = !m_visible;

	m_window->set_hidden(!m_visible);

	if (m_visible) {
		refresh();
	}
}

TreeNode *UIDebugPanel::add_view_node(View *view, TreeNode *parent_node) {
	TreeNode *node = (parent_node != nullptr) ? parent_node->add_child_node(make_label(view)) : m_tree->add_node(make_label(view));
	m_node_to_view[node] = view;

	for (const auto &child : view->get_children()) {
		if (child.get() == m_window) {
			continue; // never inspect the inspector
		}
		add_view_node(child.get(), node);
	}
	return node;
}

void UIDebugPanel::refresh() {
	if ((m_target == nullptr) || (m_tree_host == nullptr)) {
		return;
	}

	m_node_to_view.clear();
	m_tree_host->clear_children();

	m_tree = m_tree_host->add_child<TreeView>();

	for (const auto &child : m_target->get_root()->get_children()) {
		if (child.get() == m_window) {
			continue;
		}
		add_view_node(child.get(), nullptr);
	}
}

} // namespace Aquila::UI::DevTools
