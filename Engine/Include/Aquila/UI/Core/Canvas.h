#pragma once

#include "Aquila/Foundation/Timer.h"
#include "Aquila/Graphics/Core/Renderer2D.h"
#include "Aquila/UI/Core/DrawCompositor.h"
#include "Aquila/UI/Core/InputRouter.h"
#include "Aquila/UI/Core/LayoutEngine.h"
#include "Aquila/UI/Core/StyleEngine.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Style/StyleSheet.h"

namespace Aquila::GFX {
class GfxTexture;
}

namespace Aquila::UI::Core {

using namespace Aquila::UI::Rendering;

class Tooltip;
class DragGhost;

/// One UI tree with its own style sheet, layout, input and draw list.
/// Every frame the owner calls update(), then compute(), then submit_to_quad_batcher().
class Canvas {
	friend class InputRouter; // calls MarkDirty()/RequestLayout() on input

  public:
	Canvas(Uint32 width, Uint32 height);

	void on_event(Platform::Events::Event &event);
	void update(F32 delta_time);
	void compute();
	void submit_to_quad_batcher(Graphics::Renderer2D &r2d, GFX::GfxCommandList &cmd);
	void resize(Uint32 width, Uint32 height);

	StyleSheet &get_style_sheet();
	View *get_root();
	View *hit_test(Vec2 pos);
	[[nodiscard]] View *get_focused_view() const { return m_input_router.focused_view(); }
	[[nodiscard]] View *get_hovered_view() const { return m_input_router.hovered_view(); }
	void scroll_into_view(View *target);
	void set_scroll_offset(View *target, F32 offset_y);
	Uint32 get_width() const { return m_width; }
	Uint32 get_height() const { return m_height; }
	void notify_style_dirty(View *view);
	void notify_animation_started(View *view);
	void notify_draw_dirty(View *view);
	void notify_layout_dirty(View *view);
	void notify_focus_request(View *view);
	void notify_view_removed(View *view);
	/// Called for every view that leaves the tree, before it's destroyed.
	void register_removal_observer(Delegate<void(View *)> observer);
	void reload_styles();
	void mark_subtree_dirty(View *node);

	/// A click outside `popup` calls on_dismiss.
	void register_popup(View *popup, Delegate<void()> on_dismiss);
	void unregister_popup(View *popup);

	/// The view's on_update() runs every frame until unregister_tick().
	void register_tick(View *view);
	void unregister_tick(View *view);

	/// The label that follows the mouse while dragging.
	void show_drag_ghost(std::string label, Vec2 pos, GFX::GfxTexture *icon = nullptr);
	void move_drag_ghost(Vec2 pos);
	void hide_drag_ghost();

	bool is_draw_list_dirty() const { return m_draw_list_dirty; }
	void clear_draw_list_dirty() { m_draw_list_dirty = false; }

	/// Frames this canvas asks for only redraw that window (a FrameScheduler target).
	void set_frame_target(const void *target) { m_frame_target = target; }
	[[nodiscard]] const void *get_frame_target() const { return m_frame_target; }
	/// FrameScheduler::request_frame_in() with this canvas's frame target.
	void request_frame_in(F64 seconds) const;

  private:
	void mark_node_draw_dirty(View *node);
	void dismiss_popups_outside(View *hit);
	void register_internal_observers();

	struct OpenPopup {
		View *root;
		Delegate<void()> on_dismiss;
	};
	std::vector<OpenPopup> m_open_popups;
	std::vector<View *> m_ticking;
	std::vector<Delegate<void(View *)>> m_removal_observers;
	ViewRef m_scroll_target;
	ViewRef m_scroll_offset_target;
	F32 m_scroll_offset_y = 0.F;

	Unique<View> m_root;
	StyleEngine m_style_engine;
	LayoutEngine m_layout_engine;
	DrawCompositor m_draw_compositor;
	InputRouter m_input_router;
	Uint32 m_width;
	Uint32 m_height;

	F32 m_delta_time = 0.F;

	std::vector<View *> m_active_anims;

	bool m_layout_dirty = true;
	bool m_dirty = true;
	bool m_in_frame_pass = false;
	const void *m_frame_target = nullptr;
	bool m_draw_list_dirty = true; // true when draw list was rebuilt this frame

	void mark_dirty();
	void request_layout(); // mark layout dirty + request a frame (used by input/scroll)
	void style_pass();
	void animation_pass(F32 dt);
	void update_tooltip();

	DragGhost *m_drag_ghost = nullptr;

	Tooltip *m_tooltip = nullptr;
	ViewRef m_tooltip_target;
	Foundation::TimePoint m_tooltip_hover_start;
	bool m_tooltip_shown = false;
};
} // namespace Aquila::UI::Core
