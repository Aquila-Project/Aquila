#ifndef AQUILA_UI_DEVTOOLS_UI_DEBUG_PANEL_H
#define AQUILA_UI_DEVTOOLS_UI_DEBUG_PANEL_H

#include "Aquila/UI/Core/Behaviour.h"

namespace Aquila::UI::Core {
class View;
class Canvas;
class TreeView;
class TreeNode;
} // namespace Aquila::UI::Core

namespace Aquila::UI::DevTools {

class UIDebugPanel final : public Aquila::UI::Core::Behaviour {
  public:
	void define_behaviour() override;

	void set_target(Aquila::UI::Core::Canvas *target) { m_target = target; }
	void toggle();
	void refresh();

  private:
	Aquila::UI::Core::TreeNode *add_view_node(Aquila::UI::Core::View *view, Aquila::UI::Core::TreeNode *parent_node);

	Aquila::UI::Core::Canvas *m_target = nullptr;
	Aquila::UI::Core::TreeView *m_tree = nullptr;
	bool m_visible = false;

	std::unordered_map<Aquila::UI::Core::TreeNode *, Aquila::UI::Core::View *> m_node_to_view;
};

} // namespace Aquila::UI::DevTools

#endif
