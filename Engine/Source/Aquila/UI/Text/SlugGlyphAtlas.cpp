#include "Aquila/UI/Text/SlugGlyphAtlas.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/Math/Geometry/Bezier.h"
#include "Aquila/Foundation/SharedConstants.h"

namespace Aquila::UI::Text {

Unique<SlugGlyphAtlas> SlugGlyphAtlas::create(GFX::GfxContext &ctx) {
	Unique<SlugGlyphAtlas> atlas(new SlugGlyphAtlas());
	atlas->m_ctx = &ctx;

	atlas->m_curve_capacity_texels = SharedConstants::FONT_CURVE_RESERVE_ROWS * SharedConstants::FONT_TEX_WIDTH;
	atlas->m_band_capacity_texels = SharedConstants::FONT_BAND_RESERVE_ROWS * SharedConstants::FONT_TEX_WIDTH;
	atlas->m_curve_texels.assign(atlas->m_curve_capacity_texels, { 0.F, 0.F, 0.F, 0.F });
	atlas->m_band_texels.assign(atlas->m_band_capacity_texels, { 0U, 0U, 0U, 0U });

	atlas->m_curve_texture = ctx.create_texture({
		.width = SharedConstants::FONT_TEX_WIDTH,
		.height = SharedConstants::FONT_CURVE_RESERVE_ROWS,
		.format = RHI::TextureFormat::RGBA32F,
		.usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst,
		.debug_name = "FontAtlas_Curves",
	});
	atlas->m_band_texture = ctx.create_texture({
		.width = SharedConstants::FONT_TEX_WIDTH,
		.height = SharedConstants::FONT_BAND_RESERVE_ROWS,
		.format = RHI::TextureFormat::RGBA32U,
		.usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst,
		.sampler = RHI::SamplerDesc::point_sample(),
		.debug_name = "FontAtlas_Bands",
	});

	return atlas;
}

Option<Uint32> SlugGlyphAtlas::add_glyph(const GlyphOutline &outline) {
	const BandTransform transform = compute_band_transform(outline);

	const Uint32 curve_texel_base = m_curve_cursor;
	const Uint32 curve_texels_needed =
		static_cast<Uint32>(outline.curves.size()) * SharedConstants::FONT_TEXELS_PER_CURVE;

	const Uint32 band_texel_base = align_band_header_to_row(m_band_cursor);
	const GlyphBands bands = bucket_curves_into_bands(outline, transform, curve_texel_base);
	const Uint32 band_header_texels = 2 * SharedConstants::FONT_BAND_COUNT;
	const Uint32 band_texels_end = band_texel_base + band_header_texels + count_banded_curves(bands);

	const bool curves_fit = curve_texel_base + curve_texels_needed <= m_curve_capacity_texels;
	const bool bands_fit = band_texels_end <= m_band_capacity_texels;
	if (!curves_fit || !bands_fit) {
		return std::nullopt;
	}

	write_curve_texels(outline, curve_texel_base);
	m_curve_cursor = curve_texel_base + curve_texels_needed;

	write_band_texels(bands, band_texel_base);
	m_band_cursor = band_texels_end;

	const Uint32 glyph_id = static_cast<Uint32>(m_slug_glyphs.size());
	m_slug_glyphs.push_back(SlugGlyphData{
		.glyph_loc_x = band_texel_base % SharedConstants::FONT_TEX_WIDTH,
		.glyph_loc_y = band_texel_base / SharedConstants::FONT_TEX_WIDTH,
		.band_max_x = SharedConstants::FONT_BAND_MAX,
		.band_max_y = SharedConstants::FONT_BAND_MAX,
		.band_transform = { transform.scale_x, transform.scale_y, transform.offset_x, transform.offset_y },
		.em_min = outline.em_min,
		.em_max = outline.em_max,
	});

	return glyph_id;
}

SlugGlyphAtlas::BandTransform SlugGlyphAtlas::compute_band_transform(const GlyphOutline &outline) {
	const F32 em_width = outline.em_max.x - outline.em_min.x;
	const F32 em_height = outline.em_max.y - outline.em_min.y;
	const F32 band_count = static_cast<F32>(SharedConstants::FONT_BAND_COUNT);

	const F32 scale_x = (em_width > SharedConstants::FONT_AXIS_LINE_EPSILON_EM) ? (band_count / em_width) : 0.F;
	const F32 scale_y = (em_height > SharedConstants::FONT_AXIS_LINE_EPSILON_EM) ? (band_count / em_height) : 0.F;
	return {
		.scale_x = scale_x,
		.scale_y = scale_y,
		.offset_x = -outline.em_min.x * scale_x,
		.offset_y = -outline.em_min.y * scale_y,
	};
}

SlugGlyphAtlas::GlyphBands SlugGlyphAtlas::bucket_curves_into_bands(const GlyphOutline &outline,
																	const BandTransform &transform,
																	Uint32 curve_texel_base) {
	GlyphBands bands{};
	const F32 overlap = SharedConstants::FONT_BAND_OVERLAP_EM;
	const int last_band = static_cast<int>(SharedConstants::FONT_BAND_MAX);

	for (Uint32 curve_index = 0; curve_index < static_cast<Uint32>(outline.curves.size()); ++curve_index) {
		const auto &curve = outline.curves[curve_index];
		const Uint32 texel = curve_texel_base + (curve_index * SharedConstants::FONT_TEXELS_PER_CURVE);
		const Uint32 texel_x = texel % SharedConstants::FONT_TEX_WIDTH;
		const Uint32 texel_y = texel / SharedConstants::FONT_TEX_WIDTH;

		const auto bounds = Math::Geometry::Bezier::compute_bounds(curve);
		const bool is_horizontal_line = (bounds.max.y - bounds.min.y) <= SharedConstants::FONT_AXIS_LINE_EPSILON_EM;
		const bool is_vertical_line = (bounds.max.x - bounds.min.x) <= SharedConstants::FONT_AXIS_LINE_EPSILON_EM;

		if (!is_horizontal_line) {
			const int first = Math::max(
				0, static_cast<int>(Math::floor(((bounds.min.y - overlap) * transform.scale_y) + transform.offset_y)));
			const int last = Math::min(
				last_band,
				static_cast<int>(Math::floor(((bounds.max.y + overlap) * transform.scale_y) + transform.offset_y)));
			for (int band = first; band <= last; ++band) {
				bands.horizontal[static_cast<Uint32>(band)].push_back({ texel_x, texel_y, bounds.max.x });
			}
		}

		if (!is_vertical_line) {
			const int first = Math::max(
				0, static_cast<int>(Math::floor(((bounds.min.x - overlap) * transform.scale_x) + transform.offset_x)));
			const int last = Math::min(
				last_band,
				static_cast<int>(Math::floor(((bounds.max.x + overlap) * transform.scale_x) + transform.offset_x)));
			for (int band = first; band <= last; ++band) {
				bands.vertical[static_cast<Uint32>(band)].push_back({ texel_x, texel_y, bounds.max.y });
			}
		}
	}

	auto by_descending_sort_key = [](const BandedCurve &a, const BandedCurve &b) { return a.sort_key > b.sort_key; };
	for (auto &band : bands.horizontal) {
		std::ranges::sort(band, by_descending_sort_key);
	}
	for (auto &band : bands.vertical) {
		std::ranges::sort(band, by_descending_sort_key);
	}

	return bands;
}

Uint32 SlugGlyphAtlas::count_banded_curves(const GlyphBands &bands) {
	Uint32 total = 0;
	for (const auto &band : bands.horizontal) {
		total += static_cast<Uint32>(band.size());
	}
	for (const auto &band : bands.vertical) {
		total += static_cast<Uint32>(band.size());
	}
	return total;
}

Uint32 SlugGlyphAtlas::align_band_header_to_row(Uint32 band_cursor) {
	const Uint32 header_texels = 2 * SharedConstants::FONT_BAND_COUNT;
	const Uint32 column = band_cursor % SharedConstants::FONT_TEX_WIDTH;
	if (column + header_texels > SharedConstants::FONT_TEX_WIDTH) {
		return band_cursor + (SharedConstants::FONT_TEX_WIDTH - column);
	}
	return band_cursor;
}

void SlugGlyphAtlas::write_curve_texels(const GlyphOutline &outline, Uint32 curve_texel_base) {
	for (Uint32 curve_index = 0; curve_index < static_cast<Uint32>(outline.curves.size()); ++curve_index) {
		const auto &curve = outline.curves[curve_index];
		const Uint32 texel = curve_texel_base + (curve_index * SharedConstants::FONT_TEXELS_PER_CURVE);
		m_curve_texels[texel] = { curve.p0.x, curve.p0.y, curve.p1.x, curve.p1.y };
		m_curve_texels[texel + 1] = { curve.p2.x, curve.p2.y, 0.F, 0.F };
	}
}

void SlugGlyphAtlas::write_band_texels(const GlyphBands &bands, Uint32 band_texel_base) {
	const Uint32 band_header_texels = 2 * SharedConstants::FONT_BAND_COUNT;
	Uint32 index_cursor = band_texel_base + band_header_texels;

	for (Uint32 band = 0; band < SharedConstants::FONT_BAND_COUNT; ++band) {
		const Uint32 count = static_cast<Uint32>(bands.horizontal[band].size());
		const Uint32 offset = (count > 0) ? (index_cursor - band_texel_base) : 0U;
		m_band_texels[band_texel_base + band] = { count, offset, 0U, 0U };
		for (const auto &banded : bands.horizontal[band]) {
			m_band_texels[index_cursor++] = { banded.curve_texel_x, banded.curve_texel_y, 0U, 0U };
		}
	}

	for (Uint32 band = 0; band < SharedConstants::FONT_BAND_COUNT; ++band) {
		const Uint32 count = static_cast<Uint32>(bands.vertical[band].size());
		const Uint32 offset = (count > 0) ? (index_cursor - band_texel_base) : 0U;
		m_band_texels[band_texel_base + SharedConstants::FONT_BAND_COUNT + band] = { count, offset, 0U, 0U };
		for (const auto &banded : bands.vertical[band]) {
			m_band_texels[index_cursor++] = { banded.curve_texel_x, banded.curve_texel_y, 0U, 0U };
		}
	}
}

void SlugGlyphAtlas::upload() {
	m_ctx->upload_texture_data(*m_curve_texture, m_curve_texels.data(), sizeof(F32) * 4 * m_curve_texels.size());
	m_ctx->upload_texture_data(*m_band_texture, m_band_texels.data(), sizeof(Uint32) * 4 * m_band_texels.size());
}

const SlugGlyphData *SlugGlyphAtlas::get_glyph_data(Uint32 glyph_id) const {
	if (glyph_id < static_cast<Uint32>(m_slug_glyphs.size())) {
		return &m_slug_glyphs[glyph_id];
	}
	return nullptr;
}

} // namespace Aquila::UI::Text
