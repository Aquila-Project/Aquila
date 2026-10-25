#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/UI/Widgets/DragGhost.h"
#include "Aquila/UI/Widgets/Tooltip.h"

namespace Aquila::UI::Core {

using namespace Aquila::UI::Rendering;

Canvas::Canvas(Uint32 width, Uint32 height)
	: m_layout_engine(width, height), m_input_router(*this, m_draw_compositor), m_width(width), m_height(height) {
	m_root = std::make_unique<View>();
	m_root->set_skip_hit_test(true);
	m_root->set_canvas(this);

	m_draw_compositor.set_canvas_size(width, height);

	StyleProperties root_style;
	root_style.width = StyleLength::grow();
	root_style.height = StyleLength::grow();
	m_root->set_style(root_style);
	notify_style_dirty(m_root.get());
	style_pass();

	m_tooltip = m_root->add_child(std::make_unique<Tooltip>());
	m_drag_ghost = m_root->add_child(std::make_unique<DragGhost>());

	register_internal_observers();
}

Canvas::~Canvas() {
	m_root.reset();
}

void Canvas::register_internal_observers() {
	register_removal_observer([this](View *view) { m_style_engine.remove(view); });
	register_removal_observer([this](View *view) { m_input_router.on_view_removed(view); });
	register_removal_observer([this](View *view) { m_draw_compositor.forget_view(view); });
	register_removal_observer([this](View *view) { unregister_popup(view); });
	register_removal_observer([this](View *view) { unregister_tick(view); });
	register_removal_observer([this](View *view) { std::erase(m_active_anims, view); });
	register_removal_observer([this](View *view) {
		if (m_tooltip == view) {
			m_tooltip = nullptr;
			m_tooltip_shown = false;
		}
		if (m_drag_ghost == view) {
			m_drag_ghost = nullptr;
		}
	});
}

void Canvas::mark_dirty() {
	m_dirty = true;
	if (!m_in_frame_pass) {
		Aquila::Foundation::FrameScheduler::get()->request_frame(m_frame_target);
	}
}

void Canvas::request_frame_in(F64 seconds) const {
	Aquila::Foundation::FrameScheduler::get()->request_frame_in(seconds, m_frame_target);
}

void Canvas::request_layout() {
	m_layout_dirty = true;
	mark_dirty();
}

void Canvas::notify_style_dirty(View *view) {
	m_style_engine.invalidate(view);
	mark_dirty();
}

void Canvas::notify_animation_started(View *view) {
	if (std::ranges::find(m_active_anims, view) != m_active_anims.end()) {
		return;
	}
	m_active_anims.push_back(view);
	mark_dirty();
}

void Canvas::notify_draw_dirty(View *view) {
	mark_node_draw_dirty(view);
}

void Canvas::notify_layout_dirty(View *view) {
	m_layout_dirty = true;
	mark_node_draw_dirty(view);
}

void Canvas::notify_focus_request(View *view) {
	m_input_router.set_focus(view);
}

void Canvas::notify_view_removed(View *view) {
	for (const auto &observer : m_removal_observers) {
		observer(view);
	}
}

void Canvas::register_removal_observer(Delegate<void(View *)> observer) {
	m_removal_observers.push_back(std::move(observer));
}

void Canvas::show_drag_ghost(std::string label, Vec2 pos, GFX::GfxTexture *icon) {
	if (m_drag_ghost == nullptr) {
		return;
	}
	m_drag_ghost->show(std::move(label), pos, icon);
	mark_dirty();
}

void Canvas::move_drag_ghost(Vec2 pos) {
	if (m_drag_ghost == nullptr) {
		return;
	}
	m_drag_ghost->move_to(pos);
	mark_dirty();
}

void Canvas::hide_drag_ghost() {
	if (m_drag_ghost == nullptr) {
		return;
	}
	m_drag_ghost->hide();
	mark_dirty();
}

void Canvas::register_popup(View *popup, Delegate<void()> on_dismiss) {
	unregister_popup(popup);
	m_open_popups.push_back({ popup, std::move(on_dismiss) });
}

void Canvas::unregister_popup(View *popup) {
	std::erase_if(m_open_popups, [popup](const OpenPopup &p) { return p.root == popup; });
}

void Canvas::register_tick(View *view) {
	if (std::ranges::find(m_ticking, view) == m_ticking.end()) {
		m_ticking.push_back(view);
	}
}

void Canvas::unregister_tick(View *view) {
	std::erase(m_ticking, view);
}

static bool is_within(View *node, View *root) {
	for (View *v = node; v != nullptr; v = v->get_parent()) {
		if (v == root) {
			return true;
		}
	}
	return false;
}

void Canvas::dismiss_popups_outside(View *hit) {
	if (m_open_popups.empty()) {
		return;
	}
	std::vector<Delegate<void()>> to_dismiss;
	for (const auto &popup : m_open_popups) {
		if (!is_within(hit, popup.root)) {
			to_dismiss.push_back(popup.on_dismiss);
		}
	}
	for (auto &dismiss : to_dismiss) {
		if (dismiss) {
			dismiss();
		}
	}
}

void Canvas::reload_styles() {
	mark_subtree_dirty(m_root.get());
}

void Canvas::mark_subtree_dirty(View *node) {
	notify_style_dirty(node);
	for (const auto &child : node->get_children()) {
		mark_subtree_dirty(child.get());
	}
}

void Canvas::style_pass() {
	const StyleEngine::ResolveResult result = m_style_engine.resolve(m_width, m_height, m_layout_dirty);
	m_layout_dirty = result.layout_affected;
	if (result.changed) {
		mark_dirty();
	}
}

void Canvas::animation_pass(F32 dt) {
	auto it = m_active_anims.begin();
	while (it != m_active_anims.end()) {
		View *v = *it;
		v->update_animation(dt);
		mark_node_draw_dirty(v);
		if (v->is_animation_finished()) {
			it = m_active_anims.erase(it);
		} else {
			++it;
		}
	}
}

void Canvas::compute() {
	if (!m_root || !m_dirty) {
		return;
	}
	const bool outer_pass = std::exchange(m_in_frame_pass, true);

	if (m_layout_dirty) {
		constexpr int K_MAX_CONTAINER_RESOLVE_PASSES = 3;
		const bool has_container_rules = m_style_engine.get_style_sheet().has_container_blocks();

		m_layout_engine.run_layout(m_root.get(), m_delta_time);
		m_layout_dirty = false;

		for (int pass = 0;
			 has_container_rules && m_layout_engine.did_layout_resize() && pass < K_MAX_CONTAINER_RESOLVE_PASSES;
			 ++pass) {
			for (View *resized : m_layout_engine.get_resized_nodes()) {
				for (const auto &child : resized->get_children()) {
					notify_style_dirty(child.get());
				}
			}
			style_pass();
			if (!m_layout_dirty) {
				break;
			}
			m_layout_dirty = false;
			animation_pass(0.F);
			m_layout_engine.run_layout(m_root.get(), 0.F);
		}

		finish_layout();
	}

	if (m_scroll_target != nullptr) {
		m_layout_engine.scroll_into_view(m_scroll_target);
		m_scroll_target = nullptr;
		m_layout_engine.run_layout(m_root.get(), m_delta_time);
		m_layout_dirty = false;
		finish_layout();
	}

	if (m_draw_compositor.rebuild_dirty(m_root.get())) {
		m_draw_list_dirty = true;
	}
	m_dirty = false;
	m_in_frame_pass = outer_pass;
	if (m_style_engine.has_pending()) {
		Aquila::Foundation::FrameScheduler::get()->request_frame(m_frame_target);
	}
}

void Canvas::finish_layout() {
	constexpr int K_MAX_SETTLE_PASSES = 2;
	for (int pass = 0; (m_layout_dirty || m_style_engine.has_pending()) && pass < K_MAX_SETTLE_PASSES; ++pass) {
		style_pass();
		if (!m_layout_dirty) {
			break;
		}
		m_layout_dirty = false;
		animation_pass(0.F);
		m_layout_engine.run_layout(m_root.get(), 0.F);
	}
	m_layout_engine.commit_scroll_offsets();
	m_draw_compositor.recull(m_root.get());
}

void Canvas::mark_node_draw_dirty(View *node) {
	node->set_draw_dirty();
	mark_dirty();
}

void Canvas::submit_to_quad_batcher(Graphics::Renderer2D &r2d, GFX::GfxCommandList &cmd) {
	if (!m_root) {
		return;
	}
	if (m_dirty) {
		style_pass();
		compute();
	}
	m_draw_compositor.submit(r2d, cmd);
}

void Canvas::on_event(Platform::Events::Event &e) {
	m_input_router.on_event(e);
}

View *Canvas::hit_test(Vec2 pos) {
	return m_draw_compositor.hit_test(pos);
}

void Canvas::scroll_into_view(View *target) {
	m_scroll_target = target;
	m_layout_dirty = true;
	mark_dirty();
}

void Canvas::update(F32 delta_time) {
	const bool outer_pass = std::exchange(m_in_frame_pass, true);
	m_delta_time = delta_time;
	update_tooltip();
	bool needs_frame = false;
	if (!m_ticking.empty()) {
		std::vector<View *> ticking_snapshot = m_ticking;
		for (View *view : ticking_snapshot) {
			if (view->on_update(delta_time)) {
				needs_frame = true;
			}
		}
	}
	style_pass();
	animation_pass(delta_time);
	m_in_frame_pass = outer_pass;
	if (needs_frame || !m_active_anims.empty()) {
		Aquila::Foundation::FrameScheduler::get()->request_frame(m_frame_target);
	}
}

void Canvas::update_tooltip() {
	if (m_tooltip == nullptr) {
		return;
	}

	static constexpr F32 K_TOOLTIP_DELAY = 0.5F;
	static constexpr F32 K_TOOLTIP_GAP = 4.F;

	View *owner = nullptr;
	for (View *v = m_input_router.hovered_view(); v != nullptr; v = v->get_parent()) {
		if (v == m_tooltip) {
			break;
		}
		if (!v->get_tooltip().empty()) {
			owner = v;
			break;
		}
	}

	if (owner == nullptr) {
		if (m_tooltip_shown) {
			m_tooltip->hide();
			m_tooltip_shown = false;
			mark_dirty();
		}
		m_tooltip_target = nullptr;
		return;
	}

	if (owner != m_tooltip_target) {
		m_tooltip_target = owner;
		m_tooltip_hover_start = Foundation::now();
		if (m_tooltip_shown) {
			m_tooltip->hide();
			m_tooltip_shown = false;
		}
	}

	if (m_tooltip_shown) {
		return;
	}

	const F64 hovered_for = Foundation::elapsed_seconds(m_tooltip_hover_start, Foundation::now());
	if (hovered_for >= K_TOOLTIP_DELAY) {
		const Rect rect = owner->get_absolute_rect();
		const Vec2 size = m_tooltip->measure(owner->get_tooltip());
		const F32 canvas_w = static_cast<F32>(m_width);
		const F32 canvas_h = static_cast<F32>(m_height);

		Vec2 pos = { rect.position.x, rect.position.y + rect.size.y + K_TOOLTIP_GAP };

		if (pos.x + size.x > canvas_w) {
			pos.x = canvas_w - size.x;
		}
		if (pos.x < 0.F) {
			pos.x = 0.F;
		}

		if (pos.y + size.y > canvas_h) {
			const F32 above = rect.position.y - K_TOOLTIP_GAP - size.y;
			pos.y = (above >= 0.F) ? above : (canvas_h - size.y);
		}
		if (pos.y < 0.F) {
			pos.y = 0.F;
		}

		m_tooltip->show_at(pos, owner->get_tooltip());
		m_tooltip_shown = true;
		mark_dirty();
	} else {
		request_frame_in(K_TOOLTIP_DELAY - hovered_for);
	}
}

void Canvas::resize(Uint32 width, Uint32 height) {
	m_width = width;
	m_height = height;
	m_draw_compositor.set_canvas_size(width, height);
	m_layout_dirty = true;
	m_draw_list_dirty = true;
	mark_dirty();
	// @media rules depend on viewport size — re-resolve all styles on resize.
	if (m_style_engine.get_style_sheet().has_media_blocks()) {
		mark_subtree_dirty(m_root.get());
	}
	m_layout_engine.set_dimensions(width, height);
}

StyleSheet &Canvas::get_style_sheet() {
	return m_style_engine.get_style_sheet();
}

View *Canvas::get_root() {
	return m_root.get();
}

} // namespace Aquila::UI::Core
