#include "Aquila/UI/DevTools/PickerOverlay.h"

#include "Aquila/UI/Core/FontRegistry.h"
#include "Aquila/UI/Rendering/DrawList.h"
#include "Aquila/UI/Style/ComputedStyle.h"
#include "Aquila/UI/Text/FontAtlas.h"

namespace Aquila::UI::DevTools {

using namespace Aquila::UI::Core;

namespace {
constexpr float k_badge_font = 12.F;
constexpr float k_badge_pad_x = 6.F;
constexpr float k_badge_pad_y = 3.F;
} // namespace

PickerOverlay::PickerOverlay() {
	add_class("picker-overlay");
	m_should_skip_hit_test = true;

	Aquila::UI::FloatingConfig fc;
	fc.attach_to = Aquila::UI::FloatingAttachTo::Root;
	fc.element_point = Aquila::UI::FloatingAttachPoint::LeftTop;
	fc.parent_point = Aquila::UI::FloatingAttachPoint::LeftTop;
	fc.z_index = 900;
	set_floating(fc);
}

void PickerOverlay::set_target(const Rect &rect, std::string label) {
	m_active = true;
	m_target = rect;
	m_label = std::move(label);
	queue_redraw();
}

void PickerOverlay::clear() {
	if (!m_active) {
		return;
	}
	m_active = false;
	queue_redraw();
}

void PickerOverlay::on_draw_self(Aquila::UI::Rendering::DrawList &draw_list) {
	if (!m_active) {
		return;
	}

	const Vec4 accent = get_computed_style().color;
	const Vec4 fill = { accent.r, accent.g, accent.b, accent.a * 0.18F };
	draw_list.draw_rect(m_target, fill, Vec4(0.F), 1.5F, accent, 0);

	if (m_font == nullptr) {
		m_font = FontRegistry::resolve("mono");
		if (m_font == nullptr) {
			m_font = FontRegistry::resolve("regular");
		}
	}
	if (m_font == nullptr) {
		return;
	}

	const int width = static_cast<int>(std::lround(m_target.size.x));
	const int height = static_cast<int>(std::lround(m_target.size.y));
	std::string text = m_label;
	if (!text.empty()) {
		text += "   ";
	}
	text += std::to_string(width) + " × " + std::to_string(height);

	const Vec2 measured = m_font->measure_text(text, k_badge_font);
	const Vec2 badge_size = { measured.x + k_badge_pad_x * 2.F, measured.y + k_badge_pad_y * 2.F };

	Vec2 badge_pos = { m_target.position.x, m_target.position.y - badge_size.y - 2.F };
	if (badge_pos.y < 0.F) {
		badge_pos.y = m_target.position.y + 2.F;
	}

	const Rect badge_rect = { badge_pos, badge_size };
	const Vec4 badge_bg = { 0.09F, 0.09F, 0.09F, 0.94F };
	draw_list.draw_rect(badge_rect, badge_bg, Vec4(3.F), 0.F, Vec4(0.F), 1);

	const Rect text_rect = { { badge_pos.x + k_badge_pad_x, badge_pos.y + k_badge_pad_y }, measured };
	const Vec4 text_color = { 0.94F, 0.94F, 0.94F, 1.F };
	draw_list.draw_text(text_rect, text, m_font, text_color, k_badge_font, Aquila::UI::TextAlign::Left, 2);
}

} // namespace Aquila::UI::DevTools
