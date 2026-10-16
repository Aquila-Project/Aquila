#pragma once

#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Graphics/Core/Renderer2D.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Rendering/DrawList.h"

namespace Aquila::UI::Core {

using namespace Aquila::UI::Rendering;

class DrawCompositor {
  public:
	void set_canvas_size(Uint32 width, Uint32 height);

	void recull(View *root);

	bool rebuild_dirty(View *root);

	[[nodiscard]] View *hit_test(Vec2 pos) const;

	void forget_view(View *view);

	void submit(Graphics::Renderer2D &r2d, GFX::GfxCommandList &cmd);

  private:
	void rebuild_lists(View *root);
	void compose_draw_list();
	void cull(View *node, Int32 parent_effective_z, const Rect *clip_rect, F32 parent_opacity);
	void collect_layer_subtree(View *node, const Rect *clip_rect);
	void emit_floating_layer(View *node, const Rect *clip_rect, F32 parent_opacity);

	struct HitTestItem {
		View *view;
		Rect clip;
	};

	std::array<std::vector<DrawCmd>, SharedConstants::Z_RANGE> m_z_buckets;
	std::unordered_map<View *, std::vector<DrawCmd>> m_per_node_cmds;
	std::vector<HitTestItem> m_canvas_items;
	std::vector<HitTestItem> m_canvas_layers;
	std::vector<View *> m_float_roots;
	DrawList m_draw_list;
	bool m_compose_needed = false;
	Uint32 m_width = 0;
	Uint32 m_height = 0;
};

} // namespace Aquila::UI::Core
