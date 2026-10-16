#pragma once

#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/Button.h"

namespace Aquila::GFX {
class GfxTexture;
}

namespace Aquila::UI::Core {

class TreeNode;

class TreeView : public View {
  public:
	TreeView();

	[[nodiscard]] std::string_view get_type_name() const override { return "TreeView"; }

	TreeNode *add_node(std::string label);
	void remove_node(TreeNode *node);
	View *add_child(Unique<View> child) override;

	void on_drag_enter(DragState &state) override;
	void on_drag_leave(DragState &state) override;

	Signal<void(TreeNode *)> on_selected;
	Signal<void()> on_deselected;
	Signal<void(TreeNode *, Vec2)> on_node_right_clicked;
	void set_on_background_right_clicked(Delegate<void(Vec2)> callback);

	void select_node(TreeNode *node);
	void deselect();
	[[nodiscard]] TreeNode *get_selected() const { return m_selected; }

	void set_expand_icons(GFX::GfxTexture *collapsed, GFX::GfxTexture *expanded);

  protected:
	View *m_content = nullptr;

  private:
	friend class TreeNode;
	void notify_selected(TreeNode *node);
	void notify_right_clicked(TreeNode *node, Vec2 pos);
	void refresh_indicators(View *node);

	TreeNode *m_selected = nullptr;
	GFX::GfxTexture *m_icon_collapsed = nullptr;
	GFX::GfxTexture *m_icon_expanded = nullptr;
};

class TreeNode : public View {
  public:
	TreeNode(std::string label, TreeView &owner, int depth);

	[[nodiscard]] std::string_view get_type_name() const override { return "TreeNode"; }
	static constexpr ViewKind k_kind = ViewKind::TreeNode;
	[[nodiscard]] ViewKind get_kind() const override { return k_kind; }

	TreeNode *add_child_node(std::string label);
	View *add_child(Unique<View> node) override;
	void on_drag_enter(DragState &state) override;
	void on_drag_leave(DragState &state) override;
	void update_depth(int new_depth);
	void set_label(std::string label);
	void set_expanded(bool expanded);

	[[nodiscard]] bool is_expanded() const { return m_expanded; }
	[[nodiscard]] const std::string &get_label() const { return m_label; }
	[[nodiscard]] int get_depth() const { return m_depth; }

	void set_selected(bool selected);
	void set_type_icon(GFX::GfxTexture *icon);
	void refresh_indicator();

	void set_selectable(bool selectable) { m_selectable = selectable; }
	[[nodiscard]] bool is_selectable() const { return m_selectable; }
	[[nodiscard]] View *get_row() const { return m_row; }
	[[nodiscard]] View *get_actions() const { return m_actions; }
	[[nodiscard]] Button *get_header() const { return m_header; }
	[[nodiscard]] View *get_children_container() const { return m_children; }
	void track_row_hover(View *view);

	Signal<void(bool)> on_row_hover_changed;

  protected:
	TreeView &m_owner;

  private:
	void apply_state();
	void update_header_text();
	void on_header_clicked();
	void on_header_right_clicked(Vec2 pos);

	std::string m_label;
	int m_depth = 0;
	bool m_expanded = true;
	GFX::GfxTexture *m_type_icon = nullptr;
	bool m_selectable = true;
	int m_row_hover_count = 0;

	View *m_row = nullptr;
	Button *m_header = nullptr;
	View *m_actions = nullptr;
	View *m_children = nullptr;
};

} // namespace Aquila::UI::Core
