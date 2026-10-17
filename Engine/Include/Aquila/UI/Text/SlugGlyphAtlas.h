#pragma once

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxTexture.h"
#include "Aquila/UI/Text/GlyphOutline.h"

namespace Aquila::UI::Text {

struct SlugGlyphData {
	Uint32 glyph_loc_x;
	Uint32 glyph_loc_y;
	Uint32 band_max_x;
	Uint32 band_max_y;
	Vec4 band_transform;
	Vec2 em_min;
	Vec2 em_max;
};

class SlugGlyphAtlas {
  public:
	~SlugGlyphAtlas() = default;
	AQUILA_NONCOPYABLE(SlugGlyphAtlas);
	AQUILA_NONMOVEABLE(SlugGlyphAtlas);

	static Unique<SlugGlyphAtlas> create(GFX::GfxContext &ctx);

	/// nullopt when the reserved texture rows are full. Call upload() after adding.
	[[nodiscard]] Option<Uint32> add_glyph(const GlyphOutline &outline);
	void upload();

	[[nodiscard]] const SlugGlyphData *get_glyph_data(Uint32 glyph_id) const;

	[[nodiscard]] GFX::GfxTexture *get_curve_texture() const { return m_curve_texture.get(); }
	[[nodiscard]] GFX::GfxTexture *get_band_texture() const { return m_band_texture.get(); }

  private:
	SlugGlyphAtlas() = default;

	struct BandTransform {
		F32 scale_x;
		F32 scale_y;
		F32 offset_x;
		F32 offset_y;
	};

	struct BandedCurve {
		Uint32 curve_texel_x;
		Uint32 curve_texel_y;
		F32 sort_key;
	};

	using BandColumn = std::array<std::vector<BandedCurve>, SharedConstants::FONT_BAND_COUNT>;

	struct GlyphBands {
		BandColumn horizontal;
		BandColumn vertical;
	};

	static BandTransform compute_band_transform(const GlyphOutline &outline);
	static GlyphBands bucket_curves_into_bands(const GlyphOutline &outline, const BandTransform &transform,
											   Uint32 curve_texel_base);
	static Uint32 count_banded_curves(const GlyphBands &bands);
	static Uint32 align_band_header_to_row(Uint32 band_cursor);

	void write_curve_texels(const GlyphOutline &outline, Uint32 curve_texel_base);
	void write_band_texels(const GlyphBands &bands, Uint32 band_texel_base);

	GFX::GfxContext *m_ctx = nullptr;

	Ref<GFX::GfxTexture> m_curve_texture;
	Ref<GFX::GfxTexture> m_band_texture;

	std::vector<std::array<F32, 4>> m_curve_texels;
	std::vector<std::array<Uint32, 4>> m_band_texels;
	Uint32 m_curve_cursor = 0;
	Uint32 m_band_cursor = 0;
	Uint32 m_curve_capacity_texels = 0;
	Uint32 m_band_capacity_texels = 0;

	std::vector<SlugGlyphData> m_slug_glyphs;
};

} // namespace Aquila::UI::Text
