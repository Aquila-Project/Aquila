#pragma once

#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/DockDragContext.h"
#include "Aquila/UI/Widgets/DockPanel.h"
#include "Aquila/UI/Widgets/DockTypes.h"

namespace Aquila::UI::Core {

class DockNode;
class DockSplitter;
struct DockLayoutDesc;

/// Panels in tabbed nodes that can be split, moved and torn off, like an IDE.
/// Fill it from markup, apply_layout() or by hand.
class DockSpace : public View {
  public:
	DockSpace();

	[[nodiscard]] std::string_view get_type_name() const override { return "DockSpace"; }
	[[nodiscard]] DockNode *get_root_node() const { return m_root; }

	// Compiles a declared <DockNode>/<DockPanel> child tree into the runtime dock structure.
	void on_xml_loaded() override;

	using PanelFactory = Delegate<Unique<DockPanel>(const std::string &panel_id)>;

	/// Panels already open are reused by id, new ones come from the panel factory.
	/// The ones the layout doesn't place go to the first node, or get destroyed with `discard_unplaced`.
	bool apply_layout(const DockLayoutDesc &desc, bool discard_unplaced = false);

	void set_panel_factory(PanelFactory factory) { m_panel_factory = std::move(factory); }
	/// Called for every tab bar so you can add buttons to it.
	void set_tab_bar_decorator(std::function<void(DockNode *, View *)> decorator);
	void set_closable_tabs(bool closable, GFX::GfxTexture *close_icon);

	[[nodiscard]] DockNode *find_node_of(const DockPanel *panel) const;
	bool move_panel(DockPanel *panel, DockNode *target, DropZone zone = DropZone::Center);
	void close_panel(DockPanel *panel);
	void for_each_leaf(const std::function<void(DockNode *)> &visit) const;

	[[nodiscard]] bool has_any_panels() const;
	[[nodiscard]] DockNode *first_leaf_with_tabs() const;
	/// A tab dropped outside the canvas: you get its content, title and the screen position.
	void set_tear_off_callback(Delegate<void(Unique<View>, std::string, Vec2)> cb) { m_on_tear_off = std::move(cb); }

	void set_emptied_callback(Delegate<void()> cb) { m_on_emptied = std::move(cb); }

	void set_external_drag_observer(Delegate<void(Vec2)> on_outside, Delegate<void()> on_inside) {
		m_on_external_drag_move = std::move(on_outside);
		m_on_external_drag_clear = std::move(on_inside);
	}
	bool try_dock_external(Unique<View> &panel_view, const std::string &title, Vec2 local_pos);
	void preview_external_drag(Vec2 local_pos);
	void clear_external_drag();
	void begin_external_drag(DockPanel *panel, const std::string &title);

  private:
	void compile_declaration(DockNode *real_node, DockNode *decl_node);
	void realize_slot(DockNode *real_leaf, View *slot);
	void realize_panel(DockNode *real_leaf, DockPanel *decl_panel);

	void execute_drop(DockNode *target, DropZone zone, Vec2 release_pos);
	void collapse_node(DockNode *node);
	void hoist_single_child(DockNode *container, DockNode *only);
	void update_preview(DockNode *target, DropZone zone);
	void update_drag_ghost(Vec2 pos);
	void hide_drag_ghost();

	[[nodiscard]] bool is_outside_canvas(Vec2 pos) const;

	DockDragContext m_drag_ctx;
	DockNode *m_root = nullptr;
	DockNode *m_drop_target = nullptr;
	DropZone m_current_zone = DropZone::None;
	View *m_drop_preview = nullptr;
	bool m_drag_ghost_active = false;
	Delegate<void(Unique<View>, std::string, Vec2)> m_on_tear_off;
	Delegate<void()> m_on_emptied;
	Delegate<void(Vec2)> m_on_external_drag_move;
	Delegate<void()> m_on_external_drag_clear;
	bool m_drag_left_canvas = false;
	PanelFactory m_panel_factory;
};

} // namespace Aquila::UI::Core
