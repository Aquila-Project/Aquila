#include "Aquila/UI/Core/DrawCompositor.h"
#include "Aquila/Foundation/Math/Math.h"

namespace Aquila::UI::Core {

static Rendering::DrawCmd with_opacity(const Rendering::DrawCmd &cmd, F32 opacity) {
	Rendering::DrawCmd faded = cmd;
	std::visit(
		[opacity](auto &c) {
			using T = std::decay_t<decltype(c)>;
			if constexpr (std::is_same_v<T, Rendering::RectCmd>) {
				c.color.a *= opacity;
				c.border_color.a *= opacity;
			} else if constexpr (std::is_same_v<T, Rendering::ShadowCmd> || std::is_same_v<T, Rendering::TextCmd> ||
								 std::is_same_v<T, Rendering::PointCmd>) {
				c.color.a *= opacity;
			} else if constexpr (std::is_same_v<T, Rendering::ImageCmd>) {
				c.tint.a *= opacity;
			}
		},
		faded);
	return faded;
}

template <typename Emit>
static void emit_own_commands(const std::vector<Rendering::DrawCmd> &cmds, const Rect *clip_rect, F32 opacity,
							  Emit &&emit) {
	std::vector<Rect> stack;
	for (const Rendering::DrawCmd &cmd : cmds) {
		if (const auto *push = std::get_if<Rendering::ClipPushCmd>(&cmd)) {
			Rect rect = push->rect;
			if (!stack.empty()) {
				rect = stack.back().intersect(rect);
			} else if (clip_rect != nullptr) {
				rect = clip_rect->intersect(rect);
			}
			stack.push_back(rect);
			Rendering::ClipPushCmd adjusted;
			adjusted.rect = rect;
			emit(Rendering::DrawCmd(adjusted));
			continue;
		}
		if (std::holds_alternative<Rendering::ClipPopCmd>(cmd)) {
			if (!stack.empty()) {
				stack.pop_back();
			}
			Rendering::ClipPopCmd adjusted;
			adjusted.rect = !stack.empty() ? stack.back() : (clip_rect != nullptr ? *clip_rect : Rect{});
			emit(Rendering::DrawCmd(adjusted));
			continue;
		}
		emit(opacity < 1.F ? with_opacity(cmd, opacity) : cmd);
	}
}

static F32 node_opacity(const View *node) {
	return Math::clamp(node->get_display_style().opacity, 0.F, 1.F);
}

static F32 ancestor_opacity(const View *node) {
	F32 opacity = 1.F;
	for (const View *p = node->get_parent(); p != nullptr; p = p->get_parent()) {
		opacity *= node_opacity(p);
	}
	return opacity;
}

static void gather_floating_roots(View *node, std::vector<View *> &roots) {
	if (node->get_display_style().display == Display::None) {
		return;
	}
	if (node->has_floating()) {
		roots.push_back(node);
	}
	for (const auto &c : node->get_children()) {
		gather_floating_roots(c.get(), roots);
	}
}

void DrawCompositor::set_canvas_size(Uint32 width, Uint32 height) {
	m_width = width;
	m_height = height;
	m_draw_list.set_canvas_size(width, height);
}

void DrawCompositor::rebuild_lists(View *root) {
	for (auto &b : m_z_buckets) {
		b.clear();
	}
	m_canvas_items.clear();
	m_canvas_layers.clear();
	m_float_roots.clear();
	{
		const Rect canvas_bounds = { .position = { 0.F, 0.F },
									 .size = { static_cast<F32>(m_width), static_cast<F32>(m_height) } };
		cull(root, 0, &canvas_bounds, 1.F);
	}
	gather_floating_roots(root, m_float_roots);
	std::ranges::stable_sort(m_float_roots.begin(), m_float_roots.end(),
							 [](View *a, View *b) { return a->get_floating().z_index < b->get_floating().z_index; });
	for (View *float_root : m_float_roots) {
		collect_layer_subtree(float_root, nullptr);
	}
	m_compose_needed = true;
}

void DrawCompositor::compose_draw_list() {
	m_draw_list.clear();
	for (const auto &b : m_z_buckets) {
		for (const DrawCmd &cmd : b) {
			m_draw_list.append_cmd(cmd);
		}
	}
	for (View *float_root : m_float_roots) {
		emit_floating_layer(float_root, nullptr, ancestor_opacity(float_root));
	}
}

void DrawCompositor::recull(View *root) {
	rebuild_lists(root);
}

bool DrawCompositor::rebuild_dirty(View *root) {
	bool any_rebuilt = false;
	auto rebuild_node = [&](View *v) {
		if (v->is_draw_dirty()) {
			DrawList capture;
			capture.set_canvas_size(m_width, m_height);
			v->on_draw_self(capture);
			auto cmds = capture.take_commands();
			std::ranges::stable_sort(cmds.begin(), cmds.end(), [](const DrawCmd &a, const DrawCmd &b) {
				return draw_cmd_z_order(a) < draw_cmd_z_order(b);
			});
			m_per_node_cmds[v] = std::move(cmds);
			v->clear_draw_dirty();
			any_rebuilt = true;
		}
	};
	for (const HitTestItem &item : m_canvas_items) {
		rebuild_node(item.view);
	}
	for (const HitTestItem &item : m_canvas_layers) {
		rebuild_node(item.view);
	}

	if (any_rebuilt) {
		rebuild_lists(root);
	}

	if (m_compose_needed) {
		compose_draw_list();
		m_compose_needed = false;
		return true;
	}

	return false;
}

void DrawCompositor::cull(View *node, Int32 parent_effective_z, const Rect *clip_rect, F32 parent_opacity) {
	if (node->get_display_style().display == Display::None) {
		return;
	}
	if (!node->is_visible()) {
		return;
	}
	if (node->has_floating()) {
		return;
	}

	if (clip_rect != nullptr && !clip_rect->overlaps(node->get_subtree_bounds())) {
		return;
	}

	const Int32 effective_z = parent_effective_z + node->get_computed_style().z_index;
	const F32 opacity = parent_opacity * node_opacity(node);
	if (opacity <= 0.F) {
		return;
	}
	const Int32 bucket_idx =
		Math::clamp(effective_z, SharedConstants::Z_MIN, SharedConstants::Z_MAX) - SharedConstants::Z_MIN;

	if (clip_rect == nullptr || clip_rect->overlaps(node->get_absolute_rect())) {
		if (auto it = m_per_node_cmds.find(node); it != m_per_node_cmds.end()) {
			emit_own_commands(it->second, clip_rect, opacity,
							  [&](const DrawCmd &cmd) { m_z_buckets[bucket_idx].push_back(cmd); });
		}
		const Rect item_clip = clip_rect != nullptr
			? *clip_rect
			: Rect{ .position = { 0.F, 0.F }, .size = { static_cast<F32>(m_width), static_cast<F32>(m_height) } };
		m_canvas_items.push_back({ node, item_clip });
	}

	const Rect *child_clip = clip_rect;
	Rect own_clip;
	bool clips_children = false;
	const Overflow overflow = node->get_display_style().overflow;
	if (overflow != Overflow::Visible) {
		own_clip = node->get_absolute_rect();
		if (clip_rect != nullptr) {
			own_clip = clip_rect->intersect(own_clip);
			if (own_clip.is_empty()) {
				return;
			}
		}
		child_clip = &own_clip;
		clips_children = true;

		ClipPushCmd clip_push;
		clip_push.rect = own_clip;
		m_z_buckets[bucket_idx].emplace_back(clip_push);
	}

	for (const auto &child : node->get_children()) {
		cull(child.get(), effective_z, child_clip, opacity);
	}

	if (clips_children) {
		ClipPopCmd clip_pop;
		clip_pop.rect = clip_rect != nullptr ? *clip_rect : Rect{};
		m_z_buckets[bucket_idx].emplace_back(clip_pop);
	}
}

void DrawCompositor::collect_layer_subtree(View *node, const Rect *clip_rect) {
	if (node->get_display_style().display == Display::None) {
		return;
	}
	if (!node->is_visible()) {
		return;
	}
	const Rect item_clip = clip_rect != nullptr
		? *clip_rect
		: Rect{ .position = { 0.F, 0.F }, .size = { static_cast<F32>(m_width), static_cast<F32>(m_height) } };
	m_canvas_layers.push_back({ node, item_clip });

	const Rect *child_clip = clip_rect;
	Rect own_clip;
	const Overflow overflow = node->get_display_style().overflow;
	if (overflow != Overflow::Visible) {
		own_clip = node->get_absolute_rect();
		if (clip_rect != nullptr) {
			own_clip = clip_rect->intersect(own_clip);
		}
		child_clip = &own_clip;
	}

	for (const auto &child : node->get_children()) {
		if (!child->has_floating()) {
			collect_layer_subtree(child.get(), child_clip);
		}
	}
}

void DrawCompositor::emit_floating_layer(View *node, const Rect *clip_rect, F32 parent_opacity) {
	if (node->get_display_style().display == Display::None) {
		return;
	}
	if (!node->is_visible()) {
		return;
	}

	const F32 opacity = parent_opacity * node_opacity(node);
	if (opacity <= 0.F) {
		return;
	}
	if (auto it = m_per_node_cmds.find(node); it != m_per_node_cmds.end()) {
		emit_own_commands(it->second, clip_rect, opacity, [&](const DrawCmd &cmd) { m_draw_list.append_cmd(cmd); });
	}

	const Rect *child_clip = clip_rect;
	Rect own_clip;
	bool clips_children = false;
	const Overflow overflow = node->get_display_style().overflow;
	if (overflow != Overflow::Visible) {
		own_clip = node->get_absolute_rect();
		if (clip_rect != nullptr) {
			own_clip = clip_rect->intersect(own_clip);
		}
		child_clip = &own_clip;
		clips_children = true;

		ClipPushCmd clip_push;
		clip_push.rect = own_clip;
		m_draw_list.append_cmd(clip_push);
	}

	for (const auto &child : node->get_children()) {
		if (!child->has_floating()) {
			emit_floating_layer(child.get(), child_clip, opacity);
		}
	}

	if (clips_children) {
		ClipPopCmd clip_pop;
		clip_pop.rect = clip_rect != nullptr ? *clip_rect : Rect{};
		m_draw_list.append_cmd(clip_pop);
	}
}

View *DrawCompositor::hit_test(Vec2 pos) const {
	for (int i = static_cast<int>(m_canvas_layers.size()) - 1; i >= 0; --i) {
		const HitTestItem &item = m_canvas_layers[i];
		View *v = item.view;

		if (v->is_skipping_hit_test()) {
			continue;
		}

		if (!v->get_absolute_rect().contains(pos)) {
			continue;
		}
		if (!item.clip.contains(pos)) {
			continue;
		}

		View *p = v->get_parent();
		while (p != nullptr) {
			if (p->is_input_leaf() && p->get_absolute_rect().contains(pos)) {
				v = p;
			}
			p = p->get_parent();
		}
		return v;
	}

	for (int i = static_cast<int>(m_canvas_items.size()) - 1; i >= 0; --i) {
		const HitTestItem &item = m_canvas_items[i];
		View *v = item.view;

		if (v->is_skipping_hit_test()) {
			continue;
		}

		if (!v->get_absolute_rect().contains(pos)) {
			continue;
		}
		if (!item.clip.contains(pos)) {
			continue;
		}
		View *p = v->get_parent();
		while (p != nullptr) {
			if (p->is_input_leaf() && p->get_absolute_rect().contains(pos)) {
				v = p;
			}
			p = p->get_parent();
		}
		return v;
	}
	return nullptr;
}

void DrawCompositor::forget_view(View *view) {
	std::erase_if(m_canvas_items, [view](const HitTestItem &item) { return item.view == view; });
	std::erase_if(m_canvas_layers, [view](const HitTestItem &item) { return item.view == view; });
	std::erase(m_float_roots, view);
	m_per_node_cmds.erase(view);
}

void DrawCompositor::submit(Graphics::Renderer2D &r2d, GFX::GfxCommandList &cmd) {
	if (m_draw_list.is_empty()) {
		return;
	}
	m_draw_list.submit(r2d, cmd);
}

} // namespace Aquila::UI::Core
