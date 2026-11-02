#include "Aquila/UI/Text/FontFace.h"
#include "Aquila/Foundation/Math/Geometry/Bezier.h"
#include "Aquila/Foundation/Text/Utf8.h"

namespace Aquila::UI::Text {

namespace {

constexpr Uint64 K_MIN_FONT_SIZE = 12;
constexpr int K_TYPOGRAPHIC_FAMILY = 16;
constexpr int K_FAMILY = 1;

int font_offset(const Uint8 *data, Uint64 size) {
	if (data == nullptr || size < K_MIN_FONT_SIZE) {
		return -1;
	}
	const int offset = stbtt_GetFontOffsetForIndex(data, 0);
	return offset >= 0 && static_cast<Uint64>(offset) < size ? offset : -1;
}

std::string utf16be_to_utf8(const char *text, int length) {
	std::string out;
	const auto unit = [text](int at) {
		return static_cast<Uint32>((static_cast<Uint8>(text[at]) << 8U) | static_cast<Uint8>(text[at + 1]));
	};
	for (int i = 0; i + 1 < length; i += 2) {
		Uint32 codepoint = unit(i);
		if (codepoint >= 0xD800 && codepoint < 0xDC00 && i + 3 < length) {
			const Uint32 low = unit(i + 2);
			if (low >= 0xDC00 && low < 0xE000) {
				codepoint = 0x10000 + ((codepoint - 0xD800) << 10U) + (low - 0xDC00);
				i += 2;
			}
		}
		char bytes[4];
		out.append(bytes, Foundation::Utf8::encode(codepoint, bytes));
	}
	return out;
}

}

Option<std::string> FontFace::read_family_name(const Uint8 *ttf_data, Uint64 data_size) {
	const int offset = font_offset(ttf_data, data_size);
	stbtt_fontinfo info{};
	if (offset < 0 || stbtt_InitFont(&info, ttf_data, offset) == 0) {
		return std::nullopt;
	}
	for (const int name_id : { K_TYPOGRAPHIC_FAMILY, K_FAMILY }) {
		int length = 0;
		const char *name = stbtt_GetFontNameString(&info, &length, STBTT_PLATFORM_ID_MICROSOFT,
												   STBTT_MS_EID_UNICODE_BMP, STBTT_MS_LANG_ENGLISH, name_id);
		if (name != nullptr && length > 0) {
			return utf16be_to_utf8(name, length);
		}
		name = stbtt_GetFontNameString(&info, &length, STBTT_PLATFORM_ID_MAC, STBTT_MAC_EID_ROMAN,
									   STBTT_MAC_LANG_ENGLISH, name_id);
		if (name != nullptr && length > 0) {
			return std::string(name, static_cast<Usize>(length));
		}
	}
	return std::string{};
}

Unique<FontFace> FontFace::create(const Uint8 *ttf_data, Uint64 data_size) {
	const int offset = font_offset(ttf_data, data_size);
	if (offset < 0) {
		return nullptr;
	}
	Unique<FontFace> face(new FontFace());

	face->m_font_data.assign(ttf_data, ttf_data + data_size);
	if (stbtt_InitFont(&face->m_font_info, face->m_font_data.data(), offset) == 0) {
		return nullptr;
	}

	face->m_font_units_to_em = stbtt_ScaleForMappingEmToPixels(&face->m_font_info, 1.0F);

	int ascent = 0;
	int descent = 0;
	int line_gap = 0;
	stbtt_GetFontVMetrics(&face->m_font_info, &ascent, &descent, &line_gap);
	face->m_ascent_em = static_cast<F32>(ascent) * face->m_font_units_to_em;
	face->m_descent_em = static_cast<F32>(descent) * face->m_font_units_to_em;
	face->m_line_height_em = static_cast<F32>(ascent - descent + line_gap) * face->m_font_units_to_em;

	return face;
}

int FontFace::find_glyph_index(Uint32 codepoint) const {
	return stbtt_FindGlyphIndex(&m_font_info, static_cast<int>(codepoint));
}

GlyphOutline FontFace::extract_outline(int glyph_index) const {
	GlyphOutline outline;

	int box_min_x = 0;
	int box_min_y = 0;
	int box_max_x = 0;
	int box_max_y = 0;
	stbtt_GetGlyphBox(&m_font_info, glyph_index, &box_min_x, &box_min_y, &box_max_x, &box_max_y);

	outline.em_min = { 0.F, 0.F };
	outline.em_max = { static_cast<F32>(box_max_x - box_min_x) * m_font_units_to_em,
					   static_cast<F32>(box_max_y - box_min_y) * m_font_units_to_em };

	auto to_em = [&](int x, int y) -> Vec2 {
		return { static_cast<F32>(x - box_min_x) * m_font_units_to_em,
				 static_cast<F32>(y - box_min_y) * m_font_units_to_em };
	};

	auto add_curve = [&](Vec2 start, Vec2 control, Vec2 end) {
		Math::Geometry::Bezier::QuadraticBezier curve{ .p0 = start, .p1 = control, .p2 = end };
		auto split = Math::Geometry::Bezier::split_at_y_extrema(curve);
		if (split.was_split) {
			outline.curves.push_back(split.left);
			outline.curves.push_back(split.right);
		} else {
			outline.curves.push_back(curve);
		}
	};

	auto add_line = [&](Vec2 start, Vec2 end) { add_curve(start, end, end); };

	stbtt_vertex *vertices = nullptr;
	const int vertex_count = stbtt_GetGlyphShape(&m_font_info, glyph_index, &vertices);

	Vec2 cursor{};
	for (int i = 0; i < vertex_count; ++i) {
		switch (vertices[i].type) {
		case STBTT_vmove:
			cursor = to_em(vertices[i].x, vertices[i].y);
			break;

		case STBTT_vline: {
			Vec2 end = to_em(vertices[i].x, vertices[i].y);
			add_line(cursor, end);
			cursor = end;
			break;
		}

		case STBTT_vcurve: {
			Vec2 control = to_em(vertices[i].cx, vertices[i].cy);
			Vec2 end = to_em(vertices[i].x, vertices[i].y);
			add_curve(cursor, control, end);
			cursor = end;
			break;
		}

		case STBTT_vcubic: {
			Vec2 control1 = to_em(vertices[i].cx, vertices[i].cy);
			Vec2 control2 = to_em(vertices[i].cx1, vertices[i].cy1);
			Vec2 end = to_em(vertices[i].x, vertices[i].y);

			Vec2 midpoint = (cursor + 3.0F * control1 + 3.0F * control2 + end) * 0.125F;
			add_curve(cursor, (cursor + control1) * 0.5F, midpoint);
			add_curve(midpoint, (control2 + end) * 0.5F, end);
			cursor = end;
			break;
		}
		}
	}

	if (vertices != nullptr) {
		stbtt_FreeShape(&m_font_info, vertices);
	}

	return outline;
}

GlyphInfo FontFace::extract_metrics(int glyph_index, Uint32 glyph_id) const {
	int advance_width = 0;
	int left_side_bearing = 0;
	stbtt_GetGlyphHMetrics(&m_font_info, glyph_index, &advance_width, &left_side_bearing);

	int box_min_x = 0;
	int box_min_y = 0;
	int box_max_x = 0;
	int box_max_y = 0;
	stbtt_GetGlyphBox(&m_font_info, glyph_index, &box_min_x, &box_min_y, &box_max_x, &box_max_y);

	GlyphInfo info{};
	info.glyph_id = glyph_id;
	info.size_em = { static_cast<F32>(box_max_x - box_min_x) * m_font_units_to_em,
					 static_cast<F32>(box_max_y - box_min_y) * m_font_units_to_em };
	info.bearing_em = { static_cast<F32>(box_min_x) * m_font_units_to_em,
						-static_cast<F32>(box_max_y) * m_font_units_to_em };
	info.advance_em = static_cast<F32>(advance_width) * m_font_units_to_em;
	return info;
}

} // namespace Aquila::UI::Text
