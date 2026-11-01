#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Foundation/Text/Utf8.h"
#include "Aquila/UI/Rendering/DrawCmd.h"
#include "Aquila/UI/Style/StyleTypes.h"

namespace Aquila::UI::Core {

namespace {

constexpr F32 K_FIT_TOLERANCE_PX = 0.5F;

Usize next_char(std::string_view text, Usize at) {
	const Foundation::Utf8::Decoded decoded = Foundation::Utf8::decode(text, at);
	return std::min(text.size(), at + (decoded.size > 0 ? decoded.size : 1U));
}

Usize previous_char(std::string_view text, Usize at) {
	if (at == 0) {
		return 0;
	}
	--at;
	while (at > 0 && (static_cast<Uint8>(text[at]) & 0xC0U) == 0x80U) {
		--at;
	}
	return at;
}

std::string shortened(std::string_view text, Text::FontAtlas &font, F32 size, F32 width, TextOverflow overflow) {
	const std::string mark = font.get_glyph(0x2026) != nullptr ? "\u2026" : "...";
	const auto fits = [&](const std::string &candidate) {
		return font.measure_text(candidate, size).x <= width + K_FIT_TOLERANCE_PX;
	};

	if (overflow == TextOverflow::Ellipsis) {
		for (Usize keep = previous_char(text, text.size()); keep > 0; keep = previous_char(text, keep)) {
			std::string candidate = std::string(text.substr(0, keep)) + mark;
			if (fits(candidate)) {
				return candidate;
			}
		}
		return mark;
	}

	Usize head = previous_char(text, next_char(text, text.size() / 2));
	Usize tail = head;
	while (head > 0 || tail < text.size()) {
		if (head >= text.size() - tail) {
			head = previous_char(text, head);
		} else {
			tail = next_char(text, tail);
		}
		std::string candidate = std::string(text.substr(0, head)) + mark + std::string(text.substr(tail));
		if (fits(candidate)) {
			return candidate;
		}
	}
	return mark;
}

}

Label::Label(std::string text, Text::FontAtlas *font) : m_text(std::move(text)), m_font(font) {
	m_should_skip_hit_test = true;
}

void Label::set_text(std::string text) {
	if (text == m_text) {
		return;
	}
	m_text = std::move(text);
	invalidate_layout();
}

void Label::set_font(Text::FontAtlas *font) {
	if (font == m_font) {
		return;
	}
	m_font = font;
	invalidate_layout();
}

Text::FontAtlas *Label::resolve_font() const {
	if (Text::FontAtlas *css = get_resolved_font()) {
		return css;
	}
	return m_font;
}

Vec2 Label::measure(float override_font_size) const {
	Text::FontAtlas *font = resolve_font();
	if ((font == nullptr) || m_text.empty()) {
		return {};
	}

	const float render_size = (override_font_size > 0.F) ? override_font_size : get_computed_style().font_size;
	const float scale = (render_size > 0.F) ? render_size : SharedConstants::FONT_DEFAULT_SIZE;

	Vec2 dims = font->measure_text(m_text, render_size);

	const Foundation::Utf8::Decoded first = Foundation::Utf8::decode(m_text, 0);
	if (const Text::GlyphInfo *glyph = font->get_glyph(first.codepoint)) {
		dims.x -= glyph->bearing_em.x * scale;
	}
	return dims;
}

Vec2 Label::get_intrinsic_size() const {
	return measure();
}

bool Label::get_clay_text_run(ClayTextRun &out) const {
	if (get_display_style().white_space != WhiteSpace::Normal) {
		return false;
	}
	Text::FontAtlas *font = resolve_font();
	if (font == nullptr || m_text.empty()) {
		return false;
	}
	out.text = m_text;
	out.font = font;
	out.font_size = get_display_style().font_size;
	out.align = get_display_style().text_align;
	return true;
}

void Label::on_draw_self(Rendering::DrawList &draw_list) {
	View::on_draw_self(draw_list);

	Text::FontAtlas *font = resolve_font();
	if (m_text.empty() || font == nullptr) {
		return;
	}

	const Rect world_rect = { .position = get_absolute_position(), .size = get_layout_rect().size };
	const Vec4 color = get_display_style().color;
	const float font_size = get_display_style().font_size;
	const Int32 z = 3;
	const bool wrap = get_display_style().white_space == WhiteSpace::Normal;
	const TextOverflow overflow = get_display_style().text_overflow;
	const F32 size = font_size > 0.F ? font_size : SharedConstants::FONT_DEFAULT_SIZE;

	if (!wrap && overflow != TextOverflow::Clip &&
		font->measure_text(m_text, size).x > world_rect.size.x + K_FIT_TOLERANCE_PX) {
		draw_list.draw_text(world_rect, shortened(m_text, *font, size, world_rect.size.x, overflow), font, color,
							font_size, get_display_style().text_align, z, false);
		return;
	}
	draw_list.draw_text(world_rect, m_text, font, color, font_size, get_display_style().text_align, z, wrap);
}

} // namespace Aquila::UI::Core
