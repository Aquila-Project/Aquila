#include "Aquila/UI/Widgets/SelectableTextView.h"

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Foundation/Text/Utf8.h"
#include "Aquila/Platform/Events/InputEvent.h"
#include "Aquila/UI/Core/Clipboard.h"
#include "Aquila/UI/Rendering/DrawList.h"
#include "Aquila/UI/Text/FontAtlas.h"

namespace Aquila::UI::Core {

SelectableTextView::SelectableTextView() {
	add_class("selectable-text-view");
}

void SelectableTextView::add_line(std::string text, Vec4 color, Int32 tag_begin, Int32 tag_length, Vec4 tag_color) {
	m_lines.push_back(std::move(text));
	m_colors.push_back(color);
	m_tags.push_back({ tag_begin, tag_length, tag_color });
	invalidate_layout();
}

void SelectableTextView::clear() {
	m_lines.clear();
	m_colors.clear();
	m_tags.clear();
	m_rows.clear();
	m_wrapped_lines = 0;
	m_selection = {};
	invalidate_layout();
}

void SelectableTextView::remove_front(Int32 count) {
	count = std::clamp(count, 0, get_line_count());
	if (count == 0) {
		return;
	}
	m_lines.erase(m_lines.begin(), m_lines.begin() + count);
	m_colors.erase(m_colors.begin(), m_colors.begin() + count);
	m_tags.erase(m_tags.begin(), m_tags.begin() + count);
	std::erase_if(m_rows, [count](const Row &row) { return row.line < count; });
	for (Row &row : m_rows) {
		row.line -= count;
	}
	m_wrapped_lines = m_wrapped_lines > static_cast<Usize>(count) ? m_wrapped_lines - static_cast<Usize>(count) : 0;

	auto shift = [count](Text::TextPosition &position) {
		position.line -= count;
		if (position.line < 0) {
			position = { 0, 0 };
		}
	};
	shift(m_selection.anchor);
	shift(m_selection.focus);
	invalidate_layout();
}

void SelectableTextView::select_all() {
	m_selection = Text::select_all(m_lines);
	queue_redraw();
}

void SelectableTextView::clear_selection() {
	m_selection = {};
	queue_redraw();
}

std::string SelectableTextView::get_selected_text() const {
	return Text::extract_text(m_lines, m_selection);
}

F32 SelectableTextView::line_height() const {
	Text::FontAtlas *font = get_resolved_font();
	const F32 size = get_computed_style().font_size;
	const F32 scale = size > 0.F ? size : SharedConstants::FONT_DEFAULT_SIZE;
	return font != nullptr ? font->get_line_height() * scale : scale;
}

Vec2 SelectableTextView::get_intrinsic_size() const {
	return { -1.F, line_height() * static_cast<F32>(rows().size()) };
}

const std::vector<SelectableTextView::Row> &SelectableTextView::rows() const {
	const F32 width = get_layout_rect().size.x;
	Text::FontAtlas *font = get_resolved_font();
	const F32 size = get_display_style().font_size;
	if (width != m_rows_width || font != m_rows_font || size != m_rows_size) {
		m_rows.clear();
		m_wrapped_lines = 0;
		m_rows_width = width;
		m_rows_font = font;
		m_rows_size = size;
	}
	for (; m_wrapped_lines < m_lines.size(); ++m_wrapped_lines) {
		wrap_line(m_wrapped_lines, width, font, size);
	}
	return m_rows;
}

void SelectableTextView::wrap_line(Usize index, F32 width, Text::FontAtlas *font, F32 size) const {
	const std::string &text = m_lines[index];
	const Int32 line = static_cast<Int32>(index);
	const Int32 length = static_cast<Int32>(text.size());
	if (font == nullptr || width <= 0.F || length == 0) {
		m_rows.push_back({ line, 0, length });
		return;
	}

	font->ensure_glyphs(text);
	const F32 scale = size > 0.F ? size : SharedConstants::FONT_DEFAULT_SIZE;
	Int32 begin = 0;
	while (begin < length) {
		F32 x = 0.F;
		Int32 end = begin;
		Int32 after_space = -1;
		while (end < length) {
			const Foundation::Utf8::Decoded decoded = Foundation::Utf8::decode(text, static_cast<Usize>(end));
			const Int32 next = end + static_cast<Int32>(decoded.size > 0 ? decoded.size : 1U);
			const Text::GlyphInfo *glyph = font->get_glyph(decoded.codepoint);
			const F32 advance = glyph != nullptr ? glyph->advance_em * scale : 0.F;
			const bool space = decoded.codepoint == ' ';
			if (!space && end > begin && x + advance > width) {
				break;
			}
			x += advance;
			end = next;
			if (space) {
				after_space = end;
			}
		}
		if (end < length && after_space > begin) {
			end = after_space;
		}
		m_rows.push_back({ line, begin, end });
		begin = end;
	}
}

F32 SelectableTextView::width_of(std::string_view text) const {
	Text::FontAtlas *font = get_resolved_font();
	if (font == nullptr || text.empty()) {
		return 0.F;
	}
	return font->measure_text(text, get_display_style().font_size).x;
}

Text::TextPosition SelectableTextView::position_at(Vec2 canvas_pos) const {
	const std::vector<Row> &all = rows();
	if (all.empty()) {
		return {};
	}
	const Rect rect = get_absolute_rect();
	const Int32 index = std::clamp(static_cast<Int32>(Math::floor((canvas_pos.y - rect.position.y) / line_height())),
								   0, static_cast<Int32>(all.size()) - 1);
	const Row &row = all[static_cast<Usize>(index)];
	Text::FontAtlas *font = get_resolved_font();
	if (font == nullptr) {
		return { row.line, row.begin };
	}
	const F32 size = get_display_style().font_size;
	const std::string_view part = std::string_view(m_lines[static_cast<Usize>(row.line)])
									  .substr(static_cast<Usize>(row.begin), static_cast<Usize>(row.end - row.begin));
	const Int32 column = Text::column_at(part, canvas_pos.x - rect.position.x, [font, size](std::string_view text) {
		return font->measure_text(text, size).x;
	});
	return { row.line, row.begin + column };
}

Rect SelectableTextView::visible_rect() const {
	Rect visible = get_absolute_rect();
	for (const View *ancestor = get_parent(); ancestor != nullptr; ancestor = ancestor->get_parent()) {
		const UI::Overflow overflow = ancestor->get_display_style().overflow;
		if (overflow != UI::Overflow::Visible) {
			visible = visible.intersect(ancestor->get_absolute_rect());
		}
	}
	return visible;
}

void SelectableTextView::on_mouse_press(Platform::MouseButton btn, Vec2 pos) {
	View::on_mouse_press(btn, pos);
	if (btn != Platform::MouseButton::Left) {
		return;
	}
	request_focus();
	m_selecting = true;
	const Text::TextPosition hit = position_at(pos);
	m_selection = { hit, hit };
	queue_redraw();
}

void SelectableTextView::on_mouse_move(Vec2 pos) {
	if (!m_selecting || !is_pressed()) {
		return;
	}
	const Text::TextPosition hit = position_at(pos);
	if (hit != m_selection.focus) {
		m_selection.focus = hit;
		queue_redraw();
	}
}

void SelectableTextView::on_mouse_release(Platform::MouseButton btn, Vec2 pos) {
	View::on_mouse_release(btn, pos);
	m_selecting = false;
}

void SelectableTextView::on_key_press(Platform::KeyCode key, int mods) {
	if ((mods & Platform::Events::MODIFIER_CONTROL) == 0) {
		return;
	}
	if (key == Platform::KeyCode::C) {
		const std::string selected = get_selected_text();
		if (!selected.empty()) {
			Clipboard::set(selected);
		}
	} else if (key == Platform::KeyCode::A) {
		select_all();
	}
}

void SelectableTextView::on_draw_self(Rendering::DrawList &draw_list) {
	View::on_draw_self(draw_list);

	Text::FontAtlas *font = get_resolved_font();
	const std::vector<Row> &all = rows();
	if (font == nullptr || all.empty()) {
		return;
	}

	const Rect rect = get_absolute_rect();
	const Rect visible = visible_rect();
	const F32 height = line_height();
	const F32 size = get_display_style().font_size;
	const Vec4 default_color = get_display_style().color;
	const Vec4 selection_color = get_display_style().effective_selection_color();

	const Int32 row_count = static_cast<Int32>(all.size());
	const Int32 first = std::max(0, static_cast<Int32>(Math::floor((visible.position.y - rect.position.y) / height)));
	const Int32 last = std::min(row_count - 1,
								static_cast<Int32>(Math::ceil((visible.position.y + visible.size.y - rect.position.y) / height)));

	const Text::TextPosition selection_begin = m_selection.begin();
	const Text::TextPosition selection_end = m_selection.end();
	const bool has_selection = !m_selection.is_empty();

	for (Int32 index = first; index <= last; ++index) {
		const Row &row = all[static_cast<Usize>(index)];
		const Usize line = static_cast<Usize>(row.line);
		const std::string_view text = m_lines[line];
		const Int32 length = static_cast<Int32>(text.size());
		const F32 top = rect.position.y + static_cast<F32>(index) * height;
		auto x_of = [&](Int32 column) {
			return width_of(text.substr(static_cast<Usize>(row.begin), static_cast<Usize>(column - row.begin)));
		};

		if (has_selection && row.line >= selection_begin.line && row.line <= selection_end.line) {
			const Int32 from = std::max(row.begin, row.line == selection_begin.line ? selection_begin.column : 0);
			const Int32 to = std::min(row.end, row.line == selection_end.line ? selection_end.column : length);
			if (to >= from) {
				const F32 start_x = x_of(from);
				F32 end_x = x_of(to);
				if (row.line != selection_end.line && row.end == length) {
					end_x += size * 0.5F;
				}
				if (end_x > start_x) {
					draw_list.draw_rect({ { rect.position.x + start_x, top }, { end_x - start_x, height } },
										selection_color, Vec4(0.F), 0.F, Vec4(0.F), 2);
				}
			}
		}

		auto draw_segment = [&](Int32 from, Int32 to, const Vec4 &color) {
			if (to <= from) {
				return;
			}
			const F32 x = x_of(from);
			draw_list.draw_text({ { rect.position.x + x, top }, { rect.size.x - x, height } },
								text.substr(static_cast<Usize>(from), static_cast<Usize>(to - from)), font, color, size,
								UI::TextAlign::Left, 3, false);
		};

		const Vec4 color = m_colors[line].a > 0.F ? m_colors[line] : default_color;
		const Tag &tag = m_tags[line];
		const Int32 tag_end = tag.begin + tag.length;
		if (tag.length <= 0 || tag_end > length) {
			draw_segment(row.begin, row.end, color);
			continue;
		}

		const Int32 tag_from = std::clamp(tag.begin, row.begin, row.end);
		const Int32 tag_to = std::clamp(tag_end, row.begin, row.end);
		draw_segment(row.begin, tag_from, color);
		draw_segment(tag_from, tag_to, tag.color);
		draw_segment(tag_to, row.end, color);
	}
}

} // namespace Aquila::UI::Core
