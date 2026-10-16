#ifndef AQUILA_UI_DEVTOOLS_UI_DEBUG_WINDOW_H
#define AQUILA_UI_DEVTOOLS_UI_DEBUG_WINDOW_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Graphics {
class Renderer2D;
}
namespace Aquila::GFX {
class GfxCommandList;
}
namespace Aquila::Platform::Events {
class Event;
}
namespace Aquila::UI::Core {
class Canvas;
class View;
class Label;
class TextInput;
class TreeView;
class TreeNode;
} // namespace Aquila::UI::Core

namespace Aquila::UI::DevTools {

class UIDebugWindow {
  public:
	UIDebugWindow();
	~UIDebugWindow();

	void build(Aquila::UI::Core::Canvas *target, Uint32 width, Uint32 height, const std::vector<std::string> &style_paths);

	void update(F32 delta_time);
	void render(Aquila::Graphics::Renderer2D &batcher, Aquila::GFX::GfxCommandList &cmd);
	void on_event(Aquila::Platform::Events::Event &event);
	void set_frame_target(const void *target);

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

	void build_toolbar(Aquila::UI::Core::View *parent);
	void build_tree_pane(Aquila::UI::Core::View *parent);
	void build_style_pane(Aquila::UI::Core::View *parent);
	void build_box_model(Aquila::UI::Core::View *parent);

	Aquila::UI::Core::View *begin_section(Aquila::UI::Core::View *parent, const std::string &title);
	Aquila::UI::Core::Label *add_row(Aquila::UI::Core::View *section, const std::string &key);
	Aquila::UI::Core::Label *add_color_row(Aquila::UI::Core::View *section, const std::string &key,
										   Aquila::UI::Core::View **out_swatch);

	Aquila::UI::Core::TreeNode *add_view_node(Aquila::UI::Core::View *view, Aquila::UI::Core::TreeNode *parent_node);
	void populate(Aquila::UI::Core::View *view);
	void apply_filter();
	void set_swatch(Aquila::UI::Core::View *swatch, Vec4 color);

	Aquila::UI::Core::Canvas *m_target = nullptr;
	Unique<Aquila::UI::Core::Canvas> m_canvas;
	Aquila::UI::Core::View *m_ignored = nullptr;
	Aquila::UI::Core::View *m_current = nullptr;

	Aquila::UI::Core::View *m_tree_host = nullptr;
	Aquila::UI::Core::TreeView *m_tree = nullptr;
	std::unordered_map<Aquila::UI::Core::TreeNode *, Aquila::UI::Core::View *> m_node_to_view;
	std::unordered_map<Aquila::UI::Core::View *, Aquila::UI::Core::TreeNode *> m_view_to_node;

	Aquila::UI::Core::TextInput *m_search = nullptr;
	std::string m_filter;

	Aquila::UI::Core::View *m_empty_hint = nullptr;
	Aquila::UI::Core::View *m_detail_body = nullptr;
	Aquila::UI::Core::Label *m_subtitle = nullptr;
	Aquila::UI::Core::Label *m_breadcrumb = nullptr;

	std::vector<PropRow> m_rows;
	std::vector<Aquila::UI::Core::View *> m_sections;

	Aquila::UI::Core::Label *m_box_pad_top = nullptr;
	Aquila::UI::Core::Label *m_box_pad_right = nullptr;
	Aquila::UI::Core::Label *m_box_pad_bottom = nullptr;
	Aquila::UI::Core::Label *m_box_pad_left = nullptr;
	Aquila::UI::Core::Label *m_box_content = nullptr;

	Aquila::UI::Core::Label *m_v_type = nullptr;
	Aquila::UI::Core::Label *m_v_id = nullptr;
	Aquila::UI::Core::Label *m_v_classes = nullptr;
	Aquila::UI::Core::Label *m_v_state = nullptr;
	Aquila::UI::Core::Label *m_v_children = nullptr;

	Aquila::UI::Core::Label *m_v_pos = nullptr;
	Aquila::UI::Core::Label *m_v_size = nullptr;
	Aquila::UI::Core::Label *m_v_ids = nullptr;

	Aquila::UI::Core::Label *m_v_display = nullptr;
	Aquila::UI::Core::Label *m_v_position = nullptr;
	Aquila::UI::Core::Label *m_v_overflow = nullptr;
	Aquila::UI::Core::Label *m_v_flex = nullptr;
	Aquila::UI::Core::Label *m_v_justify = nullptr;
	Aquila::UI::Core::Label *m_v_align = nullptr;
	Aquila::UI::Core::Label *m_v_grow = nullptr;
	Aquila::UI::Core::Label *m_v_gap = nullptr;
	Aquila::UI::Core::Label *m_v_dims = nullptr;
	Aquila::UI::Core::Label *m_v_minmax = nullptr;
	Aquila::UI::Core::Label *m_v_aspect = nullptr;
	Aquila::UI::Core::Label *m_v_inset = nullptr;
	Aquila::UI::Core::Label *m_v_zindex = nullptr;

	Aquila::UI::Core::Label *m_v_bg = nullptr;
	Aquila::UI::Core::Label *m_v_color = nullptr;
	Aquila::UI::Core::Label *m_v_border = nullptr;
	Aquila::UI::Core::Label *m_v_radius = nullptr;
	Aquila::UI::Core::Label *m_v_opacity = nullptr;
	Aquila::UI::Core::Label *m_v_shadows = nullptr;

	Aquila::UI::Core::View *m_sw_bg = nullptr;
	Aquila::UI::Core::View *m_sw_color = nullptr;
	Aquila::UI::Core::View *m_sw_border = nullptr;

	Aquila::UI::Core::Label *m_v_fontsize = nullptr;
	Aquila::UI::Core::Label *m_v_family = nullptr;
	Aquila::UI::Core::Label *m_v_textalign = nullptr;
	Aquila::UI::Core::Label *m_v_whitespace = nullptr;
};

} // namespace Aquila::UI::DevTools

#endif
