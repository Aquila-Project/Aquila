#pragma once

#include "Aquila/Graphics/Core/Renderer2D.h"
#include "Aquila/UI/Rendering/DrawCmd.h"
#include "Aquila/UI/Text/FontAtlas.h"
#include "Aquila/UI/Style/StyleTypes.h"

namespace Aquila::UI::Rendering {

/// What views draw into. Positions are absolute canvas pixels, higher z draws on top.
class DrawList {
  public:
	void draw_rect(Rect rect, Vec4 color, Vec4 radius = Vec4(0.F), float border_width = 0.F,
				   Vec4 border_color = Vec4(0.F), Int32 z = 0, BorderStyle border_style = BorderStyle::Solid);
	void draw_line(Vec2 from, Vec2 to, float width, Vec4 color, Int32 z = 0);
	void draw_shadow(Rect widget_rect, Vec2 offset, float blur, float spread, Vec4 color, Vec4 radius, Int32 z = 0);
	/// SVG textures get rasterized at exactly the drawn pixel size.
	void draw_image(Rect rect, GFX::GfxTexture *tex, Vec4 tint = Vec4(1.F), Vec2 uv_min = Vec2(0.F),
					Vec2 uv_max = Vec2(1.F), Int32 z = 0);
	void draw_point(Vec2 position, float width, Vec4 color = Vec4(1.F), Int32 z = 0);
	/// font_size 0 means FONT_DEFAULT_SIZE.
	void draw_text(Rect bounds, std::string_view text, Text::FontAtlas *font, Vec4 color, float font_size = 0.F,
				   TextAlign align = TextAlign::Left, Int32 z = 0, bool wrap = false);
	/// Replaces the current clip until pop_clip(), it isn't intersected with it.
	/// @note A view's own commands get sorted by z, so give the push the lowest z it clips and the pop the highest.
	/// The compositor intersects a view's clips with the clip it inherits.
	void push_clip(Rect clip_rect, Int32 z = 0);
	void pop_clip(Int32 z = 0);

	void sort();

	void submit(Graphics::Renderer2D &r2d, GFX::GfxCommandList &cmd);

	void append_cmd(const DrawCmd &cmd);

	[[nodiscard]] std::vector<DrawCmd> take_commands();

	void clear();
	[[nodiscard]] bool is_empty() const;

	// TODO : DELETE, debug only
	const std::vector<DrawCmd> &get_commands() const { return m_commands; }

	void set_canvas_size(Uint32 width, Uint32 height);

  private:
	[[nodiscard]] Option<Rect> active_clip() const;

	std::vector<DrawCmd> m_commands;
	std::vector<Rect> m_clip_stack;
	Uint32 m_canvas_width = 0;
	Uint32 m_canvas_height = 0;
};

} // namespace Aquila::UI::Rendering
