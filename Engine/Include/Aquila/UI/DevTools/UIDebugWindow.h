#ifndef AQUILA_UI_DEVTOOLS_UI_DEBUG_WINDOW_H
#define AQUILA_UI_DEVTOOLS_UI_DEBUG_WINDOW_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/UI/Core/WindowBehaviour.h"

namespace Aquila::UI::Core {
class Canvas;
class View;
class Label;
class TextInput;
class TreeView;
class TreeNode;
} // namespace Aquila::UI::Core

namespace Aquila::UI::DevTools {

class UIDebugWindow final : public Aquila::UI::Core::WindowBehaviour {
  public:
	explicit UIDebugWindow(Aquila::UI::Core::Canvas &target);
	~UIDebugWindow() override;

	void define_behaviour() override;

	void refresh();
	void select_view(Aquila::UI::Core::View *view);
	void set_ignored_view(Aquila::UI::Core::View *view) { m_ignored = view; }

	Delegate<void()> on_pick_requested;
	Delegate<void(Aquila::UI::Core::View *)> on_view_highlighted;

  private:
	struct PropRow {
		Aquila::UI::Core::View *row = nullptr;
		Aquila::UI::Core::Label *value = nullptr;
		Aquila::UI::Core::View *section = nullptr;
		std::string key;
	};

	void collect_rows();
	Aquila::UI::Core::TreeNode *add_view_node(Aquila::UI::Core::View *view, Aquila::UI::Core::TreeNode *parent_node);
	void populate(Aquila::UI::Core::View *view);
	void apply_filter();
	void set_swatch(Aquila::UI::Core::View *swatch, Vec4 color);
	void set_value(std::string_view id, const std::string &text);

	Aquila::UI::Core::Canvas &m_target;
	Aquila::UI::Core::View *m_ignored = nullptr;
	Aquila::UI::Core::View *m_current = nullptr;

	Aquila::UI::Core::TreeView *m_tree = nullptr;
	std::unordered_map<Aquila::UI::Core::TreeNode *, Aquila::UI::Core::View *> m_node_to_view;
	std::unordered_map<Aquila::UI::Core::View *, Aquila::UI::Core::TreeNode *> m_view_to_node;

	std::string m_filter;

	Aquila::UI::Core::View *m_empty_hint = nullptr;
	Aquila::UI::Core::View *m_detail_body = nullptr;
	Aquila::UI::Core::Label *m_breadcrumb = nullptr;

	std::vector<PropRow> m_rows;
	std::vector<Aquila::UI::Core::View *> m_sections;
	std::unordered_map<std::string, Aquila::UI::Core::Label *> m_values;
	Aquila::UI::Core::View *m_sw_bg = nullptr;
	Aquila::UI::Core::View *m_sw_color = nullptr;
	Aquila::UI::Core::View *m_sw_border = nullptr;
};

} // namespace Aquila::UI::DevTools

#endif
