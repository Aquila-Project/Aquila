#include "Aquila/UI/Widgets/SliderField.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/UI/Widgets/DragFloat.h"
#include "Aquila/UI/Widgets/Label.h"

namespace Aquila::UI::Core {

namespace {

constexpr F32 k_track_height = 4.F;
constexpr F32 k_gradient_height = 8.F;
constexpr F32 k_handle_size = 12.F;
constexpr F32 k_tick_height = 10.F;
constexpr F32 k_snap_distance = 0.015F;
constexpr int k_gradient_segments = 48;

F32 round_to_significant(F32 value, int digits) {
	if (value == 0.F) {
		return 0.F;
	}
	const F32 magnitude = std::floor(std::log10(std::abs(value)));
	const F32 scale = std::pow(10.F, static_cast<F32>(digits - 1) - magnitude);
	return std::round(value * scale) / scale;
}

Vec4 sample_gradient(const std::vector<Vec4> &colors, F32 t) {
	if (colors.empty()) {
		return Vec4(1.F);
	}
	if (colors.size() == 1) {
		return colors.front();
	}
	const F32 position = Math::clamp(t, 0.F, 1.F) * static_cast<F32>(colors.size() - 1);
	const auto index = std::min(static_cast<Usize>(position), colors.size() - 2);
	return Math::lerp(colors[index], colors[index + 1], position - static_cast<F32>(index));
}

}

class SliderFieldTrack : public Control {
  public:
	explicit SliderFieldTrack(SliderField &field) : m_field(field) {
		set_input_leaf(true);
		set_focusable(false);
	}

	[[nodiscard]] std::string_view get_type_name() const override { return "SliderFieldTrack"; }

	void redraw() { queue_redraw(); }

	void on_mouse_press(Platform::MouseButton btn, Vec2 pos) override {
		View::on_mouse_press(btn, pos);
		if (btn == Platform::MouseButton::Left) {
			m_field.edit_from_track(track_position(pos.x));
		}
	}

	void on_mouse_move(Vec2 pos) override {
		if (is_pressed()) {
			m_field.edit_from_track(track_position(pos.x));
		}
	}

	void on_draw_self(Rendering::DrawList &draw_list) override {
		View::on_draw_self(draw_list);

		const Rect rect = get_absolute_rect();
		const auto &style = get_display_style();
		const Int32 z = 0;
		const Vec4 accent = style.effective_accent_color();
		const Vec4 ink = style.color;
		const F32 left = rect.position.x + k_handle_size * 0.5F;
		const F32 width = Math::max(rect.size.x - k_handle_size, 1.F);
		const F32 cy = rect.position.y + rect.size.y * 0.5F;
		const F32 t = m_field.to_track(m_field.get_value());
		const auto &gradient = m_field.get_gradient();

		if (!gradient.empty()) {
			const F32 segment = width / static_cast<F32>(k_gradient_segments);
			for (int i = 0; i < k_gradient_segments; ++i) {
				const F32 at = (static_cast<F32>(i) + 0.5F) / static_cast<F32>(k_gradient_segments);
				const Rect piece = {
					.position = { left + segment * static_cast<F32>(i), cy - k_gradient_height * 0.5F },
					.size = { segment + 0.5F, k_gradient_height },
				};
				const F32 round_left = i == 0 ? k_gradient_height * 0.5F : 0.F;
				const F32 round_right = i == k_gradient_segments - 1 ? k_gradient_height * 0.5F : 0.F;
				draw_list.draw_rect(piece, sample_gradient(gradient, at),
									Vec4(round_left, round_right, round_right, round_left), 0.F, Vec4(0.F), z + 1);
			}
		} else {
			const Rect track = {
				.position = { left, cy - k_track_height * 0.5F },
				.size = { width, k_track_height },
			};
			draw_list.draw_rect(track, Vec4(ink.r, ink.g, ink.b, ink.a * 0.18F), Vec4(k_track_height * 0.5F), 0.F,
								Vec4(0.F), z + 1);
			if (t > 0.F) {
				const Rect fill = { .position = track.position, .size = { width * t, k_track_height } };
				draw_list.draw_rect(fill, accent, Vec4(k_track_height * 0.5F), 0.F, Vec4(0.F), z + 1);
			}
		}

		const Vec4 tick_color = Vec4(ink.r, ink.g, ink.b, ink.a * 0.55F);
		for (const SliderField::Mark &mark : m_field.get_marks()) {
			const F32 x = left + m_field.to_track(mark.value) * width;
			const Rect tick = {
				.position = { x - 0.5F, cy - k_tick_height * 0.5F },
				.size = { 1.F, k_tick_height },
			};
			draw_list.draw_rect(tick, tick_color, Vec4(0.F), 0.F, Vec4(0.F), z + 2);
		}

		const Vec4 handle_color = gradient.empty() ? accent : sample_gradient(gradient, t);
		const Rect handle = {
			.position = { left + t * width - k_handle_size * 0.5F, cy - k_handle_size * 0.5F },
			.size = { k_handle_size, k_handle_size },
		};
		draw_list.draw_rect(handle, handle_color, Vec4(k_handle_size * 0.5F), 1.5F, ink, z + 3);
	}

  private:
	[[nodiscard]] F32 track_position(F32 x) const {
		const Rect rect = get_absolute_rect();
		const F32 left = rect.position.x + k_handle_size * 0.5F;
		const F32 width = Math::max(rect.size.x - k_handle_size, 1.F);
		return Math::clamp((x - left) / width, 0.F, 1.F);
	}

	SliderField &m_field;
};

SliderField::SliderField() {
	add_class("slider-field");

	auto *row = add_child<View>();
	row->add_class("slider-field-row");
	m_track = row->add_child<SliderFieldTrack>(*this);
	m_track->add_class("slider-field-track");
	m_box = row->add_child<DragFloat>();
	m_box->add_class("slider-field-value");

	m_caption = add_child<Label>(std::string{});
	m_caption->add_class("slider-field-caption");
	m_caption->set_hidden(true);

	m_box->on_changed.connect([this](float value) {
		m_value = value;
		refresh();
		on_changed(m_value);
	});
}

void SliderField::set_value(F32 value) {
	m_value = Math::clamp(value, m_min, m_max);
	refresh();
}

void SliderField::set_range(F32 min, F32 max) {
	m_min = min;
	m_max = max;
	m_box->set_range(min, max);
	set_value(m_value);
}

void SliderField::set_slider_range(F32 min, F32 max) {
	m_slider_min = min;
	m_slider_max = max;
	refresh();
}

void SliderField::set_logarithmic(bool logarithmic) {
	m_logarithmic = logarithmic;
	refresh();
}

void SliderField::set_precision(int decimals) {
	m_precision = std::max(0, decimals);
	refresh();
}

void SliderField::set_suffix(std::string suffix) {
	m_box->set_suffix(std::move(suffix));
}

void SliderField::set_marks(std::vector<Mark> marks) {
	m_marks = std::move(marks);
	std::ranges::sort(m_marks, {}, &Mark::value);
	refresh();
}

void SliderField::set_gradient(std::vector<Vec4> colors) {
	m_gradient = std::move(colors);
	refresh();
}

F32 SliderField::to_track(F32 value) const {
	if (m_slider_max <= m_slider_min) {
		return 0.F;
	}
	if (m_logarithmic) {
		const F32 low = std::log(Math::max(m_slider_min, 1e-9F));
		const F32 high = std::log(Math::max(m_slider_max, 1e-9F));
		if (value <= m_slider_min || high <= low) {
			return 0.F;
		}
		return Math::clamp((std::log(value) - low) / (high - low), 0.F, 1.F);
	}
	return Math::clamp((value - m_slider_min) / (m_slider_max - m_slider_min), 0.F, 1.F);
}

F32 SliderField::from_track(F32 t) const {
	const F32 clamped = Math::clamp(t, 0.F, 1.F);
	if (m_logarithmic) {
		const F32 low = std::log(Math::max(m_slider_min, 1e-9F));
		const F32 high = std::log(Math::max(m_slider_max, 1e-9F));
		return std::exp(low + clamped * (high - low));
	}
	return m_slider_min + clamped * (m_slider_max - m_slider_min);
}

void SliderField::edit_from_track(F32 t) {
	F32 value = from_track(t);
	bool snapped = false;
	for (const Mark &mark : m_marks) {
		if (std::abs(to_track(mark.value) - t) < k_snap_distance) {
			value = mark.value;
			snapped = true;
			break;
		}
	}
	if (!snapped) {
		value = m_logarithmic ? round_to_significant(value, 3) : value;
	}
	value = Math::clamp(value, m_min, m_max);
	if (value == m_value) {
		return;
	}
	m_value = value;
	refresh();
	on_changed(m_value);
}

void SliderField::refresh() {
	const F32 magnitude = std::abs(m_value);
	int decimals = m_precision;
	if (magnitude >= 100.F) {
		decimals = 0;
	} else if (magnitude >= 10.F) {
		decimals = std::min(m_precision, 1);
	} else if (magnitude >= 1.F) {
		decimals = std::min(m_precision, 2);
	}
	m_box->set_precision(decimals);
	m_box->set_value(m_value);
	if (!m_box->is_pressed()) {
		const F32 span = m_slider_max - m_slider_min;
		m_box->set_speed(m_logarithmic ? Math::max(magnitude, m_slider_min) * 0.01F
									   : (span > 0.F ? span / 300.F : 0.1F));
	}

	if (m_marks.empty()) {
		m_caption->set_hidden(true);
	} else {
		const F32 t = to_track(m_value);
		const Mark *nearest = &m_marks.front();
		for (const Mark &mark : m_marks) {
			if (std::abs(to_track(mark.value) - t) < std::abs(to_track(nearest->value) - t)) {
				nearest = &mark;
			}
		}
		const bool exact = std::abs(nearest->value - m_value) <= std::abs(nearest->value) * 0.01F;
		m_caption->set_text(m_value <= 0.F ? std::string("Off") : (exact ? "" : "~ ") + nearest->label);
		m_caption->set_hidden(false);
	}

	m_track->redraw();
}

}
