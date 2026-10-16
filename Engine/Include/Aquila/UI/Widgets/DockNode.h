#pragma once

#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/DockDragContext.h"
#include "Aquila/UI/Widgets/DockTypes.h"

namespace Aquila::GFX {
class GfxTexture;
}

namespace Aquila::UI::Core {

class DockPanel;
class DockTabButton;

class DockNode : public View {
  public:
	explicit DockNode(DockDragContext *drag_ctx = nullptr);

	[[nodiscard]] std::string_view get_type_name() const override { return "DockNode"; }
	static constexpr ViewKind k_kind = ViewKind::DockNode;
	[[nodiscard]] ViewKind get_kind() const override { return k_kind; }

	std::pair<DockNode *, DockNode *> split(SplitDirection dir, bool anchor_first = true);
	DockNode *append_leaf(SplitDirection dir);

	DockPanel *add_panel(std::string title, GFX::GfxTexture *tab_icon = nullptr);

	void apply_xml_attribute(std::string_view name, std::string_view value, IResourceResolver *resolver = nullptr) override;
	[[nodiscard]] Option<SplitDirection> get_declared_split() const { return m_declared_split; }

	Unique<View> replace_panel(DockPanel *old_panel, Unique<View> new_view);
	[[nodiscard]] int index_of(const DockPanel *panel) const;
	[[nodiscard]] View *get_tab_actions() const { return m_tab_actions; }
	void decorate_tab_bar();

	void set_active_panel(int index);
	void set_active_panel_by_ptr(DockPanel *panel);
	[[nodiscard]] int get_active_panel() const { return m_active_panel; }
	[[nodiscard]] bool is_empty() const { return m_is_leaf && m_tabs.empty(); }
	[[nodiscard]] bool is_leaf() const { return m_is_leaf; }
	[[nodiscard]] std::vector<DockPanel *> get_ordered_panels() const;
	[[nodiscard]] int get_tab_count() const { return static_cast<int>(m_tabs.size()); }
	[[nodiscard]] DockPanel *get_active_panel_ptr() const;
	[[nodiscard]] View *get_tab_bar() const { return m_tab_bar; }
	// Drag-drop
	DockNode *hit_test_node(Vec2 abs_pos);
	Unique<View> detach_panel(DockPanel *panel);
	void accept_panel(Unique<View> panel_view, std::string title, DropZone zone = DropZone::Center);

	// Close a tab (no destination — panel is destroyed). Triggers CollapseNode if empty.
	void close_panel(DockPanel *panel);

	void reorder_panel(DockPanel *panel, Vec2 cursor_pos);

	// Drop zone visual feedback
	void show_drop_zones(bool show);
	void highlight_drop_zone(DropZone zone);
	[[nodiscard]] DropZone hit_test_drop_zone(Vec2 abs_pos) const;

  private:
	View *make_zone_indicator(FloatingAttachPoint elem_pt, FloatingAttachPoint parent_pt, Vec2 offset, const char *cls);
	void apply_active_panel();
	void append_tab(DockPanel *panel, std::string title);

	DockDragContext *m_drag_ctx = nullptr;
	bool m_is_leaf = true;
	View *m_tab_bar = nullptr;
	View *m_tab_strip = nullptr;
	View *m_tab_actions = nullptr;
	View *m_panel_area = nullptr;

	View *m_zone_center = nullptr;
	View *m_zone_left = nullptr;
	View *m_zone_right = nullptr;
	View *m_zone_top = nullptr;
	View *m_zone_bottom = nullptr;

	struct Tab {
		View *wrapper = nullptr;
		DockTabButton *button = nullptr;
		DockPanel *panel = nullptr;
		std::string title;
	};
	std::vector<Tab> m_tabs;
	int m_active_panel = -1;
	Option<SplitDirection> m_declared_split;
};

} // namespace Aquila::UI::Core
