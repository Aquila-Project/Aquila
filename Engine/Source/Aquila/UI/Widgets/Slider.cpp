#include "Aquila/UI/Widgets/Slider.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/UI/Core/MarkupItem.h"

namespace Aquila::UI::Core {

static constexpr float K_TRACK_HEIGHT = 4.F;
static constexpr float K_HANDLE_SIZE = 12.F;
static constexpr float K_HANDLE_PAD = K_HANDLE_SIZE * 0.5F;

Slider::Slider() {
	set_input_leaf(true);
	add_class("slider");
}

float Slider::coerce(const float &value) const {
	float clamped = Math::clamp(value, m_min, m_max);
	if (m_step > 0.F) {
		clamped = Math::round((clamped - m_min) / m_step) * m_step + m_min;
	}
	return clamped;
}

void Slider::set_range(float min, float max) {
	m_min = min;
	m_max = max;
	set_value_without_notify(get_value());
}

void Slider::set_step(float step) {
	m_step = step;
	set_value_without_notify(get_value());
}

void Slider::apply_xml_attribute(std::string_view name, std::string_view value, IResourceResolver *resolver) {
	if (name != "min" && name != "max" && name != "value") {
		BaseField::apply_xml_attribute(name, value, resolver);
		return;
	}
	const Option<F32> number = parse_markup_number(value);
	if (!number) {
		AQUILA_LOG_WARNING("Slider: {} expects a number, got '{}'", name, value);
		return;
	}
	if (name == "min") {
		set_range(*number, m_max);
	} else if (name == "max") {
		set_range(m_min, *number);
	} else {
		m_markup_value = *number;
	}
}

void Slider::on_xml_loaded() {
	if (m_markup_value) {
		set_value_without_notify(*m_markup_value);
	}
}

float Slider::value_from_x(float x) const {
	const Rect rect = get_absolute_rect();
	const float left = rect.position.x + K_HANDLE_PAD;
	const float right = rect.position.x + rect.size.x - K_HANDLE_PAD;
	const float t = Math::clamp((x - left) / (right - left), 0.F, 1.F);
	float raw = m_min + t * (m_max - m_min);
	if (m_step > 0.F) {
		raw = Math::round((raw - m_min) / m_step) * m_step + m_min;
	}
	return Math::clamp(raw, m_min, m_max);
}

void Slider::on_mouse_press(Platform::MouseButton btn, Vec2 pos) {
	if (btn == Platform::MouseButton::Left) {
		set_value(value_from_x(pos.x));
	}
	View::on_mouse_press(btn, pos);
}

void Slider::on_mouse_move(Vec2 pos) {
	set_value(value_from_x(pos.x));
}

void Slider::on_draw_self(Rendering::DrawList &draw_list) {
	View::on_draw_self(draw_list);

	const Rect rect = get_absolute_rect();
	using namespace Rendering;
	const auto &style = get_display_style();
	const Int32 z = 0;
	const float cy = rect.position.y + rect.size.y * 0.5F;

	const float t = (m_max > m_min) ? (get_value() - m_min) / (m_max - m_min) : 0.F;

	const Vec4 accent_color = style.effective_accent_color();

	if (m_track_tex != nullptr) {
		const float track_h = rect.size.y;
		const Rect track = {
			.position = { rect.position.x, rect.position.y },
			.size = { rect.size.x, track_h },
		};
		draw_list.draw_image(track, m_track_tex, Vec4(1.F), Vec2(0.F), Vec2(1.F), z + 1);

		const float hx = rect.position.x + t * rect.size.x - K_HANDLE_SIZE * 0.5F;
		const float hy = cy - K_HANDLE_SIZE * 0.5F;
		const Rect handle = { .position = { hx, hy }, .size = { K_HANDLE_SIZE, K_HANDLE_SIZE } };
		draw_list.draw_rect(handle, accent_color, Vec4(K_HANDLE_SIZE * 0.5F), style.border_width, style.border_color,
							z + 2);
	} else {
		const Rect track = {
			.position = { rect.position.x + K_HANDLE_PAD, cy - K_TRACK_HEIGHT * 0.5F },
			.size = { rect.size.x - K_HANDLE_PAD * 2.F, K_TRACK_HEIGHT },
		};
		const Vec4 track_color = Vec4(accent_color.r, accent_color.g, accent_color.b, accent_color.a * 0.3F);
		draw_list.draw_rect(track, track_color, Vec4(K_TRACK_HEIGHT * 0.5F), 0.F, Vec4(0.F), z + 1);

		const float fill_w = track.size.x * t;
		if (fill_w > 0.F) {
			const Rect fill = { .position = track.position, .size = { fill_w, K_TRACK_HEIGHT } };
			draw_list.draw_rect(fill, accent_color, Vec4(K_TRACK_HEIGHT * 0.5F), 0.F, Vec4(0.F), z + 1);
		}

		const float hx = track.position.x + fill_w - K_HANDLE_SIZE * 0.5F;
		const float hy = cy - K_HANDLE_SIZE * 0.5F;
		const Rect handle = { .position = { hx, hy }, .size = { K_HANDLE_SIZE, K_HANDLE_SIZE } };
		draw_list.draw_rect(handle, accent_color, Vec4(K_HANDLE_SIZE * 0.5F), 0.F, Vec4(0.F), z + 2);
	}
}

} // namespace Aquila::UI::Core
