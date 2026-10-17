#ifndef AQUILA_UI_DEVTOOLS_UI_DEBUG_PANEL_H
#define AQUILA_UI_DEVTOOLS_UI_DEBUG_PANEL_H

namespace Aquila::UI::Core {
class View;
class Canvas;
class TreeView;
class TreeNode;
} // namespace Aquila::UI::Core

namespace Aquila::UI::DevTools {

class UIDebugPanel {
  public:
	void build(Aquila::UI::Core::View *overlay_root, Aquila::UI::Core::Canvas *target);
	void toggle();
	void refresh();

  private:
	Aquila::UI::Core::TreeNode *add_view_node(Aquila::UI::Core::View *view, Aquila::UI::Core::TreeNode *parent_node);

	Aquila::UI::Core::Canvas *m_target = nullptr;
	Aquila::UI::Core::View *m_window = nullptr;
	Aquila::UI::Core::View *m_tree_host = nullptr;
	Aquila::UI::Core::TreeView *m_tree = nullptr;
	bool m_visible = false;

	std::unordered_map<Aquila::UI::Core::TreeNode *, Aquila::UI::Core::View *> m_node_to_view;
};

} // namespace Aquila::UI::DevTools

#endif
