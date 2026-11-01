#include "Aquila/UI/Text/FontAtlas.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Foundation/Text/Utf8.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"

namespace Aquila::UI::Text {

bool FontAtlas::append_glyph(Uint32 codepoint) {
	if (m_glyphs.contains(codepoint) || m_missing_codepoints.contains(codepoint)) {
		return false;
	}

	const int glyph_index = m_face->find_glyph_index(codepoint);
	if (glyph_index == 0) {
		m_missing_codepoints.insert(codepoint);
		return false;
	}

	const GlyphOutline outline = m_face->extract_outline(glyph_index);
	const Option<Uint32> glyph_id = m_glyph_atlas->add_glyph(outline);
	if (!glyph_id.has_value()) {
		AQUILA_LOG_ERROR("FontAtlas: glyph atlas capacity exhausted (codepoint {}); glyph skipped", codepoint);
		m_missing_codepoints.insert(codepoint);
		return false;
	}

	m_glyphs[codepoint] = m_face->extract_metrics(glyph_index, *glyph_id);
	return true;
}

Unique<FontAtlas> FontAtlas::create(GFX::GfxContext &ctx, const Uint8 *ttf_data, Uint64 data_size) {
	Unique<FontFace> face = FontFace::create(ttf_data, data_size);
	if (face == nullptr) {
		AQUILA_LOG_ERROR("FontAtlas: the data isn't a TrueType or OpenType font");
		return nullptr;
	}
	Unique<FontAtlas> atlas(new FontAtlas());
	atlas->m_face = std::move(face);
	atlas->m_glyph_atlas = SlugGlyphAtlas::create(ctx);

	for (int i = 0; i < SharedConstants::FONT_GLYPH_COUNT; ++i) {
		atlas->append_glyph(static_cast<Uint32>(SharedConstants::FONT_FIRST_CODEPOINT + i));
	}
	atlas->m_glyph_atlas->upload();

	return atlas;
}

Unique<FontAtlas> FontAtlas::create_from_file(GFX::GfxContext &ctx, const std::string &path) {
	auto file = Platform::Filesystem::VirtualFileSystem::get()->open_file(path, AccessMode::Read, OpenMode::Binary);
	if (!file || !file->is_valid()) {
		AQUILA_LOG_ERROR("FontAtlas: cannot open '{}'", path);
		return nullptr;
	}

	const Int64 size = file->size();
	if (size <= 0) {
		return nullptr;
	}
	std::vector<Uint8> data(static_cast<Usize>(size));
	file->read(data.data(), static_cast<Usize>(size));
	return create(ctx, data.data(), static_cast<Uint64>(size));
}

const GlyphInfo *FontAtlas::get_glyph(Uint32 codepoint) const {
	auto it = m_glyphs.find(codepoint);
	return it != m_glyphs.end() ? &it->second : nullptr;
}

const SlugGlyphData *FontAtlas::get_slug_data(Uint32 glyph_id) const {
	return m_glyph_atlas->get_glyph_data(glyph_id);
}

Vec2 FontAtlas::measure_text(std::string_view text, F32 font_size) const {
	const F32 pixels_per_em = (font_size > 0.F) ? font_size : SharedConstants::FONT_DEFAULT_SIZE;

	F32 width = 0.F;
	for (Usize i = 0; i < text.size();) {
		const Foundation::Utf8::Decoded decoded = Foundation::Utf8::decode(text, i);
		i += (decoded.size > 0 ? decoded.size : 1U);
		if (const GlyphInfo *glyph = get_glyph(decoded.codepoint)) {
			width += glyph->advance_em * pixels_per_em;
		}
	}
	return { width, m_face->get_line_height() * pixels_per_em };
}

void FontAtlas::ensure_glyphs(std::string_view text) {
	bool added = false;
	for (Usize i = 0; i < text.size();) {
		const Foundation::Utf8::Decoded decoded = Foundation::Utf8::decode(text, i);
		i += (decoded.size > 0 ? decoded.size : 1U);
		added |= append_glyph(decoded.codepoint);
	}
	if (added) {
		m_glyph_atlas->upload();
	}
}

} // namespace Aquila::UI::Text
