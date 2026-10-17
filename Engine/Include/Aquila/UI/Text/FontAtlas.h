#pragma once

#include "Aquila/Foundation/Defines.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxTexture.h"
#include "Aquila/UI/Text/FontFace.h"
#include "Aquila/UI/Text/SlugGlyphAtlas.h"

namespace Aquila::UI::Text {

/// A TTF font drawn with Slug: glyphs are rendered from their curves, so text stays sharp at any size.
/// Glyphs are added on demand.
class FontAtlas {
  public:
	~FontAtlas() = default;
	AQUILA_NONCOPYABLE(FontAtlas);
	AQUILA_NONMOVEABLE(FontAtlas);

	static Unique<FontAtlas> create(GFX::GfxContext &ctx, const Uint8 *ttf_data, Uint64 data_size);
	static Unique<FontAtlas> create_from_file(GFX::GfxContext &ctx, const std::string &path);

	[[nodiscard]] const GlyphInfo *get_glyph(Uint32 codepoint) const;
	[[nodiscard]] const SlugGlyphData *get_slug_data(Uint32 glyph_id) const;

	/// Glyphs that aren't loaded yet count as zero width, call ensure_glyphs() first for new text.
	/// font_size 0 means FONT_DEFAULT_SIZE.
	[[nodiscard]] Vec2 measure_text(std::string_view text, F32 font_size) const;

	/// Adds the glyphs of `text` that aren't in the atlas yet.
	void ensure_glyphs(std::string_view text);

	[[nodiscard]] GFX::GfxTexture *get_curve_texture() const { return m_glyph_atlas->get_curve_texture(); }
	[[nodiscard]] GFX::GfxTexture *get_band_texture() const { return m_glyph_atlas->get_band_texture(); }

	[[nodiscard]] F32 get_line_height() const { return m_face->get_line_height(); }
	[[nodiscard]] F32 get_ascent() const { return m_face->get_ascent(); }
	[[nodiscard]] F32 get_descent() const { return m_face->get_descent(); }

  private:
	FontAtlas() = default;

	bool append_glyph(Uint32 codepoint);

	Unique<FontFace> m_face;
	Unique<SlugGlyphAtlas> m_glyph_atlas;

	std::unordered_map<Uint32, GlyphInfo> m_glyphs;
	std::unordered_set<Uint32> m_missing_codepoints;
};

} // namespace Aquila::UI::Text
