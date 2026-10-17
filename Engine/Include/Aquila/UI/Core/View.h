#pragma once

#include "Aquila/Foundation/Signal.h"
#include "Aquila/Platform/Input.h"
#include "Aquila/UI/Rendering/DrawList.h"
#include "Aquila/UI/Style/ComputedStyle.h"
#include "Aquila/UI/Style/StyleProperties.h"
#include "Aquila/UI/Core/DragState.h"

namespace Aquila::UI::Text {
class FontAtlas;
}

namespace Aquila::UI::Core {

class Canvas;
class IResourceResolver;

enum class ViewKind : Uint8 {
	Generic,
	TextInput,
	Collapsible,
	TreeNode,
	DockNode,
	DockPanel,
	DockSplitter,
};

struct ClayTextRun {
	std::string_view text;
	Text::FontAtlas *font = nullptr;
	F32 font_size = 0.F;
	TextAlign align = TextAlign::Left;
};

/// Base of every UI element. A view owns its children, gets styled by its classes through the
/// canvas StyleSheet, laid out by Clay, and draws itself in on_draw_self().
///
/// @code
/// class Swatch : public View {
///   public:
///     void set_color(Vec4 color) {
///         if (color != m_color) {
///             m_color = color;
///             queue_redraw();
///         }
///     }
///     void on_draw_self(Rendering::DrawList &draw_list) override {
///         draw_list.draw_rect(get_absolute_rect(), m_color);
///     }
///
///   private:
///     Vec4 m_color{ 1.F };
/// };
/// @endcode
class View {
  public:
	View();
	AQUILA_NONCOPYABLE(View);
	AQUILA_NONMOVEABLE(View);

	virtual View *add_child(Unique<View> child);

	template <typename T, typename... Args> T *add_child(Args &&...args) {
		return static_cast<T *>(add_child(std::make_unique<T>(std::forward<Args>(args)...)));
	}

	/// Destroys it. detach_child() hands it back to you instead.
	void remove_child(View *child);
	Unique<View> detach_child(View *child);
	/// Destroys `old`. Null when `old` isn't a child.
	View *replace_child(View *old, Unique<View> new_child);
	/// Moves `child` right before `before`, or to the end when `before` is null.
	void reorder_child(View *child, View *before);
	[[nodiscard]] View *get_parent() const;
	/// Starts from this view, not its parent. Same for get_first_parent_that_accepts_drop().
	[[nodiscard]] View *get_first_draggable_parent() const;
	[[nodiscard]] View *get_first_parent_that_accepts_drop() const;
	[[nodiscard]] const std::vector<Unique<View>> &get_children() const;
	/// Depth first, this view included.
	View *find_by_id(std::string_view id);

	template <typename T> T *find_by_id(std::string_view id) {
		View *v = find_by_id(id);
		return v ? dynamic_cast<T *>(v) : nullptr;
	}

	void set_canvas(Canvas *canvas);
	[[nodiscard]] Canvas *get_canvas() const { return m_canvas; }

	[[nodiscard]] virtual std::string_view get_type_name() const { return "View"; }
	[[nodiscard]] virtual ViewKind get_kind() const { return ViewKind::Generic; }
	[[nodiscard]] const std::string &get_id() const { return m_id; }
	[[nodiscard]] const std::vector<std::string> &get_classes() const { return m_classes; }
	[[nodiscard]] const StyleProperties &get_style() const { return m_style; }
	[[nodiscard]] const ComputedStyle &get_computed_style() const { return m_computed_style; }
	/// What's drawn right now, mid transition. get_computed_style() is where the transition ends up.
	[[nodiscard]] const ComputedStyle &get_display_style() const { return m_display_style; }
	[[nodiscard]] Platform::CursorType get_cursor() const { return m_computed_style.cursor; }
	[[nodiscard]] const Rect &get_layout_rect() const { return m_layout_rect; }
	[[nodiscard]] Vec2 get_absolute_position() const { return m_absolute_position; }

	void add_class(std::string cls);
	void remove_class(std::string_view cls);

	void set_class(std::string cls, bool enabled);
	/// Toggles the "hidden" class.
	void set_hidden(bool hidden);

	void set_id(std::string id) { m_id = std::move(id); }
	void set_style(StyleProperties props) { m_style = std::move(props); }

	// Optional hover tooltip. Empty (the default) means this view has no tooltip.
	void set_tooltip(std::string text) { m_tooltip = std::move(text); }
	[[nodiscard]] const std::string &get_tooltip() const { return m_tooltip; }

	/// Inline style on top of what the style sheet gives, like a style="" attribute.
	void merge_style(const StyleProperties &overlay);
	void set_computed_style(ComputedStyle style);
	void set_layout_rect(Rect rect) {
		if (m_layout_rect == rect) {
			return;
		}
		m_layout_rect = rect;
		mark_subtree_bounds_dirty();
	}
	void set_absolute_position(Vec2 pos) {
		if (m_absolute_position == pos) {
			return;
		}
		m_absolute_position = pos;
		mark_subtree_bounds_dirty();
	}
	void set_clay_id(Uint32 id) { m_clay_id = id; }

	void set_layout_home(Vec2 pos) { m_layout_home = pos; }
	[[nodiscard]] Vec2 get_layout_home() const { return m_layout_home; }
	void set_layout_anim_offset(Vec2 offset) { m_layout_anim_offset = offset; }
	[[nodiscard]] Vec2 get_layout_anim_offset() const { return m_layout_anim_offset; }

	/// Hit testing stops here, the view takes the input for its whole subtree.
	void set_input_leaf(bool v) { m_is_input_leaf = v; }
	/// Clicks go through it to whatever is below.
	void set_skip_hit_test(bool v) { m_should_skip_hit_test = v; }
	[[nodiscard]] bool is_input_leaf() const { return m_is_input_leaf; }
	[[nodiscard]] bool is_visible() const { return m_visible; }

	void set_enabled(bool enabled);
	[[nodiscard]] bool is_enabled() const { return m_enabled; }

	[[nodiscard]] Text::FontAtlas *get_resolved_font() const { return m_resolved_font; }

	void request_focus();
	/// Scrolling over it also reaches the engine, the viewport uses it so the camera can dolly.
	void set_pass_through_scroll(bool v) { m_pass_through_scroll = v; }
	[[nodiscard]] bool get_pass_through_scroll() const { return m_pass_through_scroll; }

	[[nodiscard]] Rect get_absolute_rect() const { return { m_absolute_position, m_layout_rect.size }; }

	void mark_subtree_bounds_dirty() {
		m_subtree_bounds_dirty = true;
		if (m_parent != nullptr) {
			m_parent->mark_subtree_bounds_dirty();
		}
	}
	/// This view and all its descendants together, cached until something moves.
	const Rect &get_subtree_bounds();

	Signal<void(Vec2)> on_context_menu;
	Signal<void(Vec2)> on_pressed;
	Signal<void()> on_mouse_entered;
	Signal<void()> on_mouse_left;
	bool is_animation_finished() const { return m_is_animation_finished; }

	[[nodiscard]] bool is_hovered() const { return m_is_hovered; }
	[[nodiscard]] bool is_pressed() const { return m_is_pressed; }
	[[nodiscard]] bool is_focused() const { return m_is_focused; }
	[[nodiscard]] bool is_skipping_hit_test() const { return m_should_skip_hit_test; }
	[[nodiscard]] bool is_accepting_payload() const { return m_is_accepting_payload; }
	[[nodiscard]] bool is_draggable() const { return m_is_draggable; }
	[[nodiscard]] Uint32 get_clay_id() const { return m_clay_id; }

	[[nodiscard]] Uint32 get_stable_id() const { return m_stable_id; }
	/// Expires when the view is destroyed. ViewRef is built on it.
	[[nodiscard]] WeakRef<void> alive_token() const { return m_alive; }

	/// The size the view wants, -1 on an axis means no preference.
	virtual Vec2 get_intrinsic_size() const { return { -1.F, -1.F }; }

	// When true, the layout engine emits a Clay text element for this node (so Clay
	// performs word wrapping and computes multi-line height) instead of pinning a
	// fixed intrinsic box. out is only valid when this returns true.
	virtual bool get_clay_text_run(ClayTextRun & /*out*/) const { return false; }

	virtual void on_draw_self(Rendering::DrawList &draw_list);
	virtual void on_mouse_enter();
	virtual void on_mouse_leave();
	virtual void on_mouse_press(Platform::MouseButton btn, Vec2 pos);
	virtual void on_mouse_release(Platform::MouseButton btn, Vec2 pos);
	virtual void on_key_press(Platform::KeyCode key, int mods = 0) {}
	virtual void on_key_release(Platform::KeyCode key) {}
	/// Only while the view is pressed, for dragging. For hover use on_mouse_hover().
	virtual void on_mouse_move(Vec2 pos) {}
	/// Every mouse move while hovered. Override it when the view has its own hover state.
	virtual void on_mouse_hover(Vec2 pos) {}
	/// Return true to eat the scroll, otherwise it goes up to the parents.
	virtual bool on_scroll(Vec2 delta) { return false; }
	virtual void on_char_input(Uint32 codepoint) {}
	virtual void on_focus_gained();
	virtual void on_focus_lost();
	virtual void on_drag_start(DragState &d_state);
	virtual void on_drop(DragState &d_state);
	virtual void on_drag_enter(DragState &d_state);
	virtual void on_drag_leave(DragState &d_state);

	virtual void on_style_resolved();

	/// Called every frame while the view is registered with Canvas::register_tick().
	/// Return true to ask for another frame.
	virtual bool on_update(F32 delta_time) {
		(void)delta_time;
		return false;
	}

	/// LayoutLoader calls it for every attribute. Handle yours and pass the rest to the base.
	virtual void apply_xml_attribute(std::string_view name, std::string_view value,
									 IResourceResolver *resolver = nullptr);

	/// The text between the tags.
	virtual void apply_xml_text_content(std::string_view text) { (void)text; }

	/// After every attribute and child of the element was applied.
	virtual void on_xml_loaded() {}

	virtual void set_font(Text::FontAtlas * /*font*/) {}

	/// The view draws again on the next frame, and that frame gets requested.
	/// @note Never call it unconditionally from per-frame code, the app would render forever.
	/// Compare your inputs first.
	void queue_redraw();

	void set_floating(FloatingConfig cfg) { m_floating = cfg; }
	void clear_floating() { m_floating.reset(); }
	bool has_floating() const { return m_floating.has_value(); }
	const FloatingConfig &get_floating() const { return *m_floating; }

	virtual View *hit_test_absolute(Vec2 canvas_pos);

	void set_draw_dirty() { m_draw_dirty = true; }
	void clear_draw_dirty() { m_draw_dirty = false; }
	[[nodiscard]] bool is_draw_dirty() const { return m_draw_dirty; }

	void set_effective_z(Int32 z) { m_effective_z = z; }
	[[nodiscard]] Int32 get_effective_z() const { return m_effective_z; }

	/// Layout runs again, for when the view's size changed.
	void invalidate_layout();

	void update_animation(F32 delta_time);
	virtual ~View() = default;

  protected:
	bool m_is_hovered = false;
	bool m_is_pressed = false;
	bool m_is_focused = false;
	bool m_should_skip_hit_test = false;
	bool m_is_accepting_payload = false;
	bool m_is_draggable = false;

  private:
	void notify_removed(View *node);
	void mark_style_dirty();

	View *m_parent = nullptr;
	std::vector<Unique<View>> m_children;

	ComputedStyle m_display_style;
	ComputedStyle m_animation_from;
	F32 m_transition_timer = 0.F;
	bool m_display_style_initialized = false;

	std::string m_id;
	std::string m_tooltip;
	std::vector<std::string> m_classes;
	StyleProperties m_style;
	ComputedStyle m_computed_style;
	Rect m_layout_rect;
	Vec2 m_absolute_position;
	Vec2 m_layout_home{};
	Vec2 m_layout_anim_offset{};
	bool m_visible = true;
	bool m_enabled = true;
	bool m_is_input_leaf = false;
	bool m_pass_through_scroll = false;
	Uint32 m_clay_id = 0;

	Option<FloatingConfig> m_floating;
	void apply_floating_attribute(std::string_view name, std::string_view value);

	bool m_is_animation_finished = false;
	bool m_draw_dirty = true;
	Int32 m_effective_z = 0;
	Rect m_subtree_bounds{};
	bool m_subtree_bounds_dirty = true;
	Text::FontAtlas *m_resolved_font = nullptr;
	Canvas *m_canvas = nullptr;
	Uint32 m_stable_id = 0;
	Ref<char> m_alive = std::make_shared<char>(0);
};

/// A View pointer that turns null when the view is destroyed.
///
/// @code
/// ViewRef target = canvas.get_hovered_view();
/// if (target) {
///     target->set_tooltip("hi");
/// }
/// @endcode
class ViewRef {
  public:
	ViewRef() = default;
	ViewRef(View *view) { assign(view); }
	ViewRef &operator=(View *view) {
		assign(view);
		return *this;
	}

	[[nodiscard]] View *get() const { return m_alive.expired() ? nullptr : m_ptr; }
	operator View *() const { return get(); }
	View *operator->() const { return get(); }

  private:
	void assign(View *view);

	View *m_ptr = nullptr;
	WeakRef<void> m_alive;
};

/// Cheap type check through get_kind(), T needs a static k_kind. view_cast() is the cast.
template <typename T> [[nodiscard]] bool view_is(const View *view) {
	return view != nullptr && view->get_kind() == T::k_kind;
}

template <typename T> [[nodiscard]] T *view_cast(View *view) {
	return view_is<T>(view) ? static_cast<T *>(view) : nullptr;
}
} // namespace Aquila::UI::Core
