#include "Aquila/UI/Widgets/DragFloat.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/UI/Rendering/DrawCmd.h"

namespace Aquila::UI::Core {

DragFloat::DragFloat() {
	set_input_leaf(true);
	add_class("drag-float");
}

DragFloat::DragFloat(const Config &config) : DragFloat() {
	set_range(config.min, config.max);
	set_step(config.step);
	set_speed(config.speed);
	set_precision(config.precision);
	set_prefix(config.prefix);
}

void DragFloat::set_value(float value) {
	m_value = Math::clamp(value, m_min, m_max);
	if (m_step > 0.F) {
		m_value = Math::round(m_value / m_step) * m_step;
	}
	queue_redraw();
}

void DragFloat::set_range(float min, float max) {
	m_min = min;
	m_max = max;
	set_value(m_value);
}

void DragFloat::set_step(float step) {
	m_step = step;
}

void DragFloat::set_speed(float pixels_per_unit) {
	m_speed = pixels_per_unit;
}

void DragFloat::set_precision(int decimals) {
	m_precision = std::max(0, decimals);
	queue_redraw();
}

void DragFloat::set_prefix(std::string prefix) {
	m_prefix = std::move(prefix);
	queue_redraw();
}

void DragFloat::set_label(std::string label) {
	m_label = std::move(label);
	queue_redraw();
}

void DragFloat::set_suffix(std::string suffix) {
	m_suffix = std::move(suffix);
	queue_redraw();
}

void DragFloat::set_slider(bool slider) {
	m_slider = slider;
	set_class("drag-float-slider", slider);
	queue_redraw();
}

std::string DragFloat::format_value() const {
	std::ostringstream ss;
	ss.precision(m_precision);
	const float half_unit = 0.5F * Math::pow(10.F, -static_cast<float>(m_precision));
	ss << std::fixed << (Math::abs(m_value) < half_unit ? 0.F : m_value);
	return m_prefix + ss.str();
}

Text::FontAtlas *DragFloat::resolve_font() const {
	return get_resolved_font();
}

void DragFloat::enter_edit_mode() {
	m_mode = Mode::Edit;
	m_edit_state.set_text(format_value());
	m_edit_state.select_all();
	queue_redraw();
}

void DragFloat::commit_edit() {
	try {
		const float parsed = std::stof(m_edit_state.text);
		set_value(parsed);
		on_value_committed();
		on_changed(m_value);
	} catch (...) {
		// Restore last valid value on parse failure.
	}
	m_mode = Mode::Drag;
	queue_redraw();
}

void DragFloat::cancel_edit() {
	m_mode = Mode::Drag;
	queue_redraw();
}

void DragFloat::on_mouse_press(Platform::MouseButton btn, Vec2 pos) {
	View::on_mouse_press(btn, pos);
	if (btn != Platform::MouseButton::Left) {
		return;
	}
	if (m_mode == Mode::Edit) {
		return;
	}
	m_drag_start_value = m_value;
	m_drag_start_x = pos.x;
	m_has_dragged = false;
}

void DragFloat::on_mouse_release(Platform::MouseButton btn, Vec2 pos) {
	if (btn == Platform::MouseButton::Left) {
		if (m_mode == Mode::Drag && !m_has_dragged) {
			enter_edit_mode();
		}
	}
	View::on_mouse_release(btn, pos);
}

void DragFloat::on_mouse_move(Vec2 pos) {
	if (m_mode != Mode::Drag || !m_is_pressed) {
		return;
	}
	const float delta = (pos.x - m_drag_start_x) * m_speed;
	if (!m_has_dragged && Math::abs(pos.x - m_drag_start_x) > K_DRAG_THRESHOLD) {
		m_has_dragged = true;
	}
	if (m_has_dragged) {
		set_value(m_drag_start_value + delta);
		on_value_committed();
		on_changed(m_value);
	}
}

void DragFloat::on_key_press(Platform::KeyCode key, int mods) {
	if (m_mode == Mode::Edit) {
		if (key == Platform::KeyCode::Enter || key == Platform::KeyCode::Tab) {
			commit_edit();
			return;
		}
		if (key == Platform::KeyCode::Escape) {
			cancel_edit();
			return;
		}
		if (m_edit_state.handle_key_press(key, mods)) {
			queue_redraw();
		}
		return;
	}
	View::on_key_press(key, mods);
}

void DragFloat::on_char_input(Uint32 codepoint) {
	if (m_mode == Mode::Edit) {
		if (m_edit_state.handle_char_input(codepoint)) {
			queue_redraw();
		}
	}
}

void DragFloat::on_focus_lost() {
	if (m_mode == Mode::Edit) {
		commit_edit();
	}
	View::on_focus_lost();
}

void DragFloat::on_draw_self(Rendering::DrawList &draw_list) {
	View::on_draw_self(draw_list);

	using namespace Rendering;
	const Rect rect = get_absolute_rect();
	const auto &style = get_display_style();
	const Int32 z = 1;
	const float font_size = style.font_size > 0.F ? style.font_size : 14.F;

	Text::FontAtlas *font = resolve_font();
	if (font == nullptr) {
		return;
	}

	const float scale = font_size;
	const float line_h = font->get_line_height() * scale;
	const float text_y = rect.position.y + (rect.size.y - line_h) * 0.5F;
	const float pad_left = style.padding.left.resolve(0.F);
	const float pad_right = style.padding.right.resolve(0.F);
	const Rect text_rect = {
		.position = { rect.position.x + pad_left, text_y },
		.size = { rect.size.x - pad_left - pad_right, line_h },
	};

	if (m_mode == Mode::Edit) {
		if (m_is_focused && m_edit_state.has_selection()) {
			const float x0 =
				text_rect.position.x + m_edit_state.measure_to_pos(*font, scale, m_edit_state.selection_min());
			const float x1 =
				text_rect.position.x + m_edit_state.measure_to_pos(*font, scale, m_edit_state.selection_max());
			const Rect sel_rect = { .position = { x0, text_y }, .size = { x1 - x0, line_h } };
			const Vec4 sel_color = style.effective_selection_color();
			draw_list.draw_rect(sel_rect, sel_color, Vec4(2.F), 0.F, Vec4(0.F), z);
		}

		if (!m_edit_state.text.empty()) {
			draw_list.draw_text(text_rect, m_edit_state.text, font, style.color, font_size, TextAlign::Left, z + 1);
		}

		if (m_is_focused && !m_edit_state.has_selection()) {
			const float cx = text_rect.position.x + m_edit_state.measure_to_pos(*font, scale, m_edit_state.cursor);
			const Rect cursor = { .position = { cx - 0.75F, text_y }, .size = { 1.5F, line_h } };
			draw_list.draw_rect(cursor, style.color, Vec4(0.F), 0.F, Vec4(0.F), z + 2);
		}
	} else {
		// Drag mode: show the formatted value, optionally a subtle drag indicator.
		const std::string display = m_suffix.empty() ? format_value() : format_value() + " " + m_suffix;
		const bool has_range = m_min > -1e17F && m_max < 1e17F && m_max > m_min;
		if (m_slider && has_range) {
			const float fraction = Math::clamp((m_value - m_min) / (m_max - m_min), 0.F, 1.F);
			const float inset = style.border_width;
			const Rect fill = {
				.position = { rect.position.x + inset, rect.position.y + inset },
				.size = { (rect.size.x - (inset * 2.F)) * fraction, rect.size.y - (inset * 2.F) },
			};
			if (fill.size.x > 0.5F) {
				const Vec4 radius = style.border_radius;
				const bool full = fraction >= 0.999F;
				draw_list.draw_rect(fill, style.effective_accent_color(),
									Vec4(radius.x, full ? radius.y : 0.F, full ? radius.z : 0.F, radius.w), 0.F,
									Vec4(0.F), z);
			}
		}

		const TextAlign align = style.text_align;
		if (!m_label.empty()) {
			const Vec4 label_color = Vec4(style.color.r, style.color.g, style.color.b, style.color.a * 0.55F);
			draw_list.draw_text(text_rect, m_label, font, label_color, font_size, TextAlign::Left, z + 1);
			Rect value_rect = text_rect;
			if (align != TextAlign::Right) {
				const float shift = font->measure_text(m_label, font_size).x + style.gap;
				value_rect.position.x += shift;
				value_rect.size.x = std::max(0.F, value_rect.size.x - shift);
			}
			draw_list.draw_text(value_rect, display, font, style.color, font_size, align, z + 1);
			return;
		}

		draw_list.draw_text(text_rect, display, font, style.color, font_size, align, z + 1);
		if (m_slider || align != TextAlign::Center) {
			return;
		}

		// Small arrows hint that the field is draggable.
		constexpr float K_ARROW_SIZE = 4.F;
		const float cy = rect.position.y + rect.size.y * 0.5F;
		const Vec4 arrow_color = Vec4(style.color.r, style.color.g, style.color.b, style.color.a * 0.4F);
		const Rect left_arrow = {
			.position = { rect.position.x + 3.F, cy - K_ARROW_SIZE * 0.5F },
			.size = { K_ARROW_SIZE, K_ARROW_SIZE },
		};
		const Rect right_arrow = {
			.position = { rect.position.x + rect.size.x - K_ARROW_SIZE - 3.F, cy - K_ARROW_SIZE * 0.5F },
			.size = { K_ARROW_SIZE, K_ARROW_SIZE },
		};
		draw_list.draw_rect(left_arrow, arrow_color, Vec4(1.F), 0.F, Vec4(0.F), z + 2);
		draw_list.draw_rect(right_arrow, arrow_color, Vec4(1.F), 0.F, Vec4(0.F), z + 2);
	}
}

} // namespace Aquila::UI::Core
