#include "Aquila/UI/Core/StyleEngine.h"
#include "Aquila/Foundation/Profiler.h"
#include "Aquila/UI/Style/StylePropertyList.h"

namespace Aquila::UI::Core {

static bool affects_layout(const ComputedStyle &a, const ComputedStyle &b) {
#define AQ_STYLE_PROP(css, sp, cs, layout, anim, inherit) \
	AQ_STYLE_WHEN(layout, if (a.cs != b.cs) { return true; })
	AQ_STYLE_PROPERTY_LIST
#undef AQ_STYLE_PROP
	return false;
}

void StyleEngine::invalidate(View *view) {
	m_style_cache.invalidate(view);
	m_dirty_views.mark_dirty(view);
}

void StyleEngine::remove(View *view) {
	m_style_cache.remove(view);
	m_dirty_views.remove(view);
}

StyleEngine::ResolveResult StyleEngine::resolve(Uint32 viewport_width, Uint32 viewport_height, bool layout_already_dirty) {
	ResolveResult result;
	result.layout_affected = layout_already_dirty;
	if (m_dirty_views.is_empty()) {
		return result;
	}
	PROFILE_SCOPE("StyleEngine::Resolve");

	auto depth = [](const View *view) {
		int result = 0;
		while (view->get_parent()) {
			view = view->get_parent();
			result++;
		}
		return result;
	};

	std::vector<View *> queue = m_dirty_views.get_ordered();
	std::ranges::stable_sort(queue.begin(), queue.end(), [&](View *a, View *b) { return depth(a) < depth(b); });
	std::unordered_set<View *> in_queue(queue.begin(), queue.end());

	for (size_t i = 0; i < queue.size(); i++) {
		View *node = queue[i];

		const ComputedStyle *parent_style = nullptr;
		if (View *parent = node->get_parent()) {
			parent_style = m_style_cache.peek(parent);
			if (parent_style == nullptr) {
				parent_style = &parent->get_computed_style();
			}
			m_style_cache.register_dependency(node, parent);
		}

		StyleSheet::ResolveContext ctx;
		ctx.viewport_size = { static_cast<F32>(viewport_width), static_cast<F32>(viewport_height) };
		if (const View *parent = node->get_parent()) {
			ctx.container_size = parent->get_layout_rect().size;
		}

		const ComputedStyle old = node->get_computed_style();
		const ComputedStyle &resolved =
			m_style_cache.get(node, [&]() { return m_style_sheet.resolve(*node, parent_style, ctx); });

		if (resolved != old) {
			result.changed = true;
			if (!result.layout_affected && affects_layout(old, resolved)) {
				result.layout_affected = true;
			}
			node->set_computed_style(resolved);
			node->on_style_resolved();
			node->set_draw_dirty();

			for (const auto &child : node->get_children()) {
				View *c = child.get();
				if (in_queue.insert(c).second) {
					queue.push_back(c);
				}
			}
		} else {
			node->set_computed_style(resolved);
			node->on_style_resolved();
		}
	}

	m_dirty_views.clear();
	return result;
}

} // namespace Aquila::UI::Core
