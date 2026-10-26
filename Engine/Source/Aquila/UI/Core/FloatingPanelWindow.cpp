#include "Aquila/UI/Core/FloatingPanelWindow.h"

#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/DockNode.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/DockSpace.h"

namespace Aquila::UI::Core {

FloatingPanelWindow::FloatingPanelWindow() = default;
FloatingPanelWindow::~FloatingPanelWindow() = default;

void FloatingPanelWindow::build(Unique<View> panel_subtree, const std::string &title, Uint32 width, Uint32 height,
								LayoutLoader &loader) {
	m_title = title;
	Canvas &canvas = create_canvas(width, height, loader);

	auto *root = canvas.get_root();
	root->add_class("floating-panel-root");

	m_body = root->add_child<View>();
	m_body->add_class("floating-panel-body");
	m_dock_space = m_body->add_child<DockSpace>();
	m_dock_space->get_root_node()->accept_panel(std::move(panel_subtree), title);

	canvas.reload_styles();
}

bool FloatingPanelWindow::has_content() const {
	return (m_dock_space != nullptr) && m_dock_space->has_any_panels();
}

Unique<View> FloatingPanelWindow::detach_content() {
	if (m_dock_space == nullptr) {
		return nullptr;
	}

	DockNode *leaf = m_dock_space->first_leaf_with_tabs();
	if (leaf == nullptr) {
		return nullptr;
	}

	return leaf->detach_panel(leaf->get_active_panel_ptr());
}

} // namespace Aquila::UI::Core
