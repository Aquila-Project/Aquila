#pragma once

#include "stb/stb_truetype.h"

#include "Aquila/Foundation/Defines.h"
#include "Aquila/UI/Text/GlyphOutline.h"

namespace Aquila::UI::Text {

struct GlyphInfo {
	Uint32 glyph_id;
	Vec2 size_em;
	Vec2 bearing_em;
	F32 advance_em;
};

/// Glyph outlines and metrics from a TTF. Everything is in em, multiply by the font size for pixels.
class FontFace {
  public:
	~FontFace() = default;
	AQUILA_NONCOPYABLE(FontFace);
	AQUILA_NONMOVEABLE(FontFace);

	static Unique<FontFace> create(const Uint8 *ttf_data, Uint64 data_size);

	[[nodiscard]] int find_glyph_index(Uint32 codepoint) const;
	[[nodiscard]] GlyphOutline extract_outline(int glyph_index) const;
	[[nodiscard]] GlyphInfo extract_metrics(int glyph_index, Uint32 glyph_id) const;

	[[nodiscard]] F32 get_line_height() const { return m_line_height_em; }
	[[nodiscard]] F32 get_ascent() const { return m_ascent_em; }
	[[nodiscard]] F32 get_descent() const { return m_descent_em; }

  private:
	FontFace() = default;

	std::vector<Uint8> m_font_data;
	stbtt_fontinfo m_font_info{};
	F32 m_font_units_to_em = 0.F;

	F32 m_line_height_em = 0.F;
	F32 m_ascent_em = 0.F;
	F32 m_descent_em = 0.F;
};

} // namespace Aquila::UI::Text
