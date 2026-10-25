#pragma once

#include "Aquila/UI/Core/View.h"

namespace Aquila::UI::Core {

class LayoutEngine {
  public:
	LayoutEngine(Uint32 width, Uint32 height);
	~LayoutEngine();

	void set_dimensions(Uint32 width, Uint32 height);

	void run_layout(View *root, float delta_time);

	// Adjusts the scroll offset of target's nearest scrolling ancestor so target is visible.
	// Must be called after RunLayout so rects and Clay scroll state are current.
	void scroll_into_view(View *target);

	/// Makes the offsets the last layout clamped stick. Call it once after the last layout of a frame, not between
	/// passes, or a pass that sees the content shorter than it ends up eats the offset.
	void commit_scroll_offsets();

	[[nodiscard]] bool did_layout_resize() const { return m_size_changed; }

	[[nodiscard]] const std::vector<View *> &get_resized_nodes() const { return m_resized_nodes; }

  private:
	void layout_pass(View *node);
	void update_rects(View *node, Vec2 parent_clay_pos = {}, Vec2 accumulated_offset = {});
	[[nodiscard]] std::vector<View *> flow_children(View *node) const;
	[[nodiscard]] std::vector<Uint32> compute_wrap_lines(View *node) const;
	[[nodiscard]] bool wrap_lines_changed() const;
	[[nodiscard]] bool intrinsic_sizes_changed() const;
	[[nodiscard]] F32 resolve_offset(const StyleLength &len, F32 parent_size) const;

	void *m_clay_ctx = nullptr;
	std::vector<Uint8> m_clay_memory;
	Uint32 m_width;
	Uint32 m_height;
	bool m_size_changed = false;
	std::vector<View *> m_resized_nodes;
	std::vector<View *> m_scroll_views;
	std::unordered_map<const View *, std::vector<Uint32>> m_wrap_lines;
	std::unordered_map<const View *, Vec2> m_intrinsic_sizes;
};

} // namespace Aquila::UI::Core
