#include "Aquila/UI/DevTools/UIDebugPanel.h"

#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/Button.h"
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

void UIDebugPanel::define_behaviour() {
	m_tree = get<TreeView>("tree");
	if (auto *refresh_button = get<Button>("refresh")) {
		refresh_button->on_click.connect([this] { refresh(); });
	}
}

void UIDebugPanel::toggle() {
	m_visible = !m_visible;
	root().set_hidden(!m_visible);

	if (m_visible) {
		refresh();
	}
}

TreeNode *UIDebugPanel::add_view_node(View *view, TreeNode *parent_node) {
	TreeNode *node = (parent_node != nullptr) ? parent_node->add_child_node(make_label(view)) : m_tree->add_node(make_label(view));
	m_node_to_view[node] = view;

	for (const auto &child : view->get_children()) {
		if (child.get() == &root()) {
			continue; // never inspect the inspector
		}
		add_view_node(child.get(), node);
	}
	return node;
}

void UIDebugPanel::refresh() {
	if (m_target == nullptr || m_tree == nullptr) {
		return;
	}

	m_node_to_view.clear();
	m_tree->clear();

	for (const auto &child : m_target->get_root()->get_children()) {
		if (child.get() == &root()) {
			continue;
		}
		add_view_node(child.get(), nullptr);
	}
}

} // namespace Aquila::UI::DevTools
