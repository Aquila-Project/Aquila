#include "Aquila/UI/Widgets/TextInput.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Rendering/DrawCmd.h"
#include "Aquila/Platform/Events/InputEvent.h"
#include "Aquila/Platform/Input.h"

namespace Aquila::UI::Core {

static void init_text_input(TextInput *self) {
	self->set_input_leaf(true);
	self->add_class("text-input");
}

TextInput::TextInput() {
	init_text_input(this);
}

TextInput::TextInput(std::string placeholder) : m_placeholder(std::move(placeholder)) {
	init_text_input(this);
}

void TextInput::set_text(const std::string &text) {
	m_state.set_text(text);
	queue_redraw();
}

void TextInput::set_font(Text::FontAtlas *font) {
	m_font = font;
	queue_redraw();
}

void TextInput::set_placeholder(std::string text) {
	m_placeholder = std::move(text);
	queue_redraw();
}

Text::FontAtlas *TextInput::resolve_font() const {
	if (Text::FontAtlas *css = get_resolved_font()) {
		return css;
	}
	return m_font;
}

void TextInput::clamp_scroll_offset(Text::FontAtlas *font, float scale, float visible_width) {
	const float cursor_x = m_state.measure_to_pos(*font, scale, m_state.cursor);
	if (cursor_x - m_scroll_offset_x < 0.F) {
		m_scroll_offset_x = cursor_x;
	} else if (cursor_x - m_scroll_offset_x > visible_width) {
		m_scroll_offset_x = cursor_x - visible_width;
	}
	m_scroll_offset_x = Math::max(0.F, m_scroll_offset_x);
}

void TextInput::on_mouse_press(Platform::MouseButton btn, Vec2 pos) {
	View::on_mouse_press(btn, pos);
	if (btn != Platform::MouseButton::Left) {
		return;
	}

	Text::FontAtlas *font = resolve_font();
	if (font == nullptr) {
		return;
	}

	const float font_size = get_display_style().font_size;
	const float scale = (font_size > 0.F) ? font_size : SharedConstants::FONT_DEFAULT_SIZE;

	constexpr float k_pad_x = 4.F;
	const float visible_width = get_layout_rect().size.x - k_pad_x * 2.F;
	const float local_x = pos.x - get_absolute_position().x - k_pad_x + m_scroll_offset_x;
	const size_t hit = m_state.hit_test_pos(*font, scale, local_x);

	const bool shift = Platform::Input::is_key_pressed(Platform::KeyCode::LeftShift) ||
		Platform::Input::is_key_pressed(Platform::KeyCode::RightShift);

	if (shift) {
		m_state.cursor = hit;
	} else {
		m_state.cursor = hit;
		m_state.select_anchor = hit;
	}
	clamp_scroll_offset(font, scale, visible_width);
	reset_blink();
	queue_redraw();
}

void TextInput::on_mouse_move(Vec2 pos) {
	if (!m_is_pressed) {
		return;
	}
	Text::FontAtlas *font = resolve_font();
	if (font == nullptr) {
		return;
	}

	const float font_size = get_display_style().font_size;
	const float scale = (font_size > 0.F) ? font_size : SharedConstants::FONT_DEFAULT_SIZE;

	constexpr float k_pad_x = 4.F;
	const float visible_width = get_layout_rect().size.x - k_pad_x * 2.F;
	const float local_x = pos.x - get_absolute_position().x - k_pad_x + m_scroll_offset_x;
	const size_t hit = m_state.hit_test_pos(*font, scale, local_x);
	if (hit != m_state.cursor) {
		m_state.cursor = hit;
		clamp_scroll_offset(font, scale, visible_width);
		queue_redraw();
	}
}

void TextInput::on_key_press(Platform::KeyCode key, int mods) {
	const bool handled = m_state.handle_key_press(key, mods);
	if (handled) {
		on_changed(m_state.text);
		if (Text::FontAtlas *font = resolve_font()) {
			const float font_size = get_display_style().font_size;
			const float scale = (font_size > 0.F) ? font_size : SharedConstants::FONT_DEFAULT_SIZE;
			constexpr float k_pad_x = 4.F;
			const float visible_width = get_layout_rect().size.x - k_pad_x * 2.F;
			clamp_scroll_offset(font, scale, visible_width);
		}
		reset_blink();
		queue_redraw();
		return;
	}

	if (key == Platform::KeyCode::Enter) {
		on_submit(m_state.text);
	}
}

void TextInput::on_char_input(Uint32 codepoint) {
	if (m_ignore_next_char) {
		m_ignore_next_char = false;
		return;
	}
	if (m_state.handle_char_input(codepoint)) {
		on_changed(m_state.text);
		reset_blink();
		queue_redraw();
	}
}

void TextInput::on_focus_gained() {
	View::on_focus_gained();
	reset_blink();
	if (Canvas *canvas = get_canvas()) {
		canvas->register_tick(this);
	}
	queue_redraw();
}

void TextInput::on_focus_lost() {
	View::on_focus_lost();
	m_state.select_anchor = m_state.cursor;
	m_scroll_offset_x = 0.F;
	if (Canvas *canvas = get_canvas()) {
		canvas->unregister_tick(this);
	}
	queue_redraw();
}

bool TextInput::on_update(F32) {
	constexpr F64 k_blink_period = 0.53;
	const F64 elapsed = Foundation::elapsed_seconds(m_blink_start, Foundation::now());
	const auto phase = static_cast<Int64>(elapsed / k_blink_period);
	const bool visible = phase % 2 == 0;
	if (visible != m_caret_visible) {
		m_caret_visible = visible;
		queue_redraw();
	}
	if (Canvas *canvas = get_canvas(); canvas != nullptr && is_focused()) {
		canvas->request_frame_in((static_cast<F64>(phase + 1) * k_blink_period) - elapsed);
	}
	return false;
}

void TextInput::reset_blink() {
	m_blink_start = Foundation::now();
	m_caret_visible = true;
}

Vec2 TextInput::get_intrinsic_size() const {
	Vec2 result{};
	auto &style = get_computed_style();

	Text::FontAtlas *font = resolve_font();
	const float scale = (style.font_size > 0.F) ? style.font_size : SharedConstants::FONT_DEFAULT_SIZE;

	result.x = -1; // we dont care bout width
	result.y = font->get_line_height() * scale;

	return result;
}

void TextInput::on_draw_self(Rendering::DrawList &draw_list) {
	View::on_draw_self(draw_list);

	using namespace Rendering;
	const Rect rect = get_absolute_rect();
	const auto &style = get_computed_style();
	const Int32 z = 0;
	const float font_size = style.font_size > 0.F ? style.font_size : 14.F;

	Text::FontAtlas *font = resolve_font();
	if (font == nullptr) {
		return;
	}

	const float scale = font_size;
	const float line_h = font->get_line_height() * scale;
	const float k_pad_x = style.padding.left.value;
	const float k_pad_y = style.padding.top.value;
	const float content_h = rect.size.y - (k_pad_y * 2.0F);
	const float text_y = rect.position.y + k_pad_y + ((content_h - line_h) * 0.5F);

	const Rect text_rect = {
    .position = {
        rect.position.x + k_pad_x - m_scroll_offset_x,
        text_y,
    },
    .size = {
        rect.size.x - (k_pad_x * 2.0F),
        line_h,
    },
};
	const Rect clip = {
		.position = { rect.position.x + k_pad_x - 1.F, rect.position.y },
		.size = { Math::max(rect.size.x - (k_pad_x * 2.F) + 2.F, 0.F), rect.size.y },
	};
	draw_list.push_clip(clip, z + 1);

	if (m_is_focused && m_state.has_selection()) {
		const float x0 = text_rect.position.x + m_state.measure_to_pos(*font, scale, m_state.selection_min());
		const float x1 = text_rect.position.x + m_state.measure_to_pos(*font, scale, m_state.selection_max());
		const Rect sel_rect = {
			.position = { x0, text_rect.position.y },
			.size = { x1 - x0, line_h },
		};
		const Vec4 sel_color = style.effective_selection_color();
		draw_list.draw_rect(sel_rect, sel_color, Vec4(2.F), 0.F, Vec4(0.F), z + 1);
	}

	if (!m_state.text.empty()) {
		draw_list.draw_text(text_rect, m_state.text, font, style.color, font_size, TextAlign::Left, z + 1);
	} else if (!m_placeholder.empty()) {
		const Vec4 muted = style.effective_placeholder_color();
		draw_list.draw_text(text_rect, m_placeholder, font, muted, font_size, TextAlign::Left, z + 1);
	}

	if (m_is_focused && !m_state.has_selection() && m_caret_visible) {
		const float cx = text_rect.position.x + m_state.measure_to_pos(*font, scale, m_state.cursor);
		const float cy = text_rect.position.y;

		draw_list.draw_line({ cx, cy }, { cx, cy + line_h }, 0.5F, style.color, z + 2);
	}

	draw_list.pop_clip(z + 2);
}

} // namespace Aquila::UI::Core
