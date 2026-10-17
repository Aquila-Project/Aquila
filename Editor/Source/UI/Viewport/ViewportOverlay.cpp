#include "UI/Viewport/ViewportOverlay.h"

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/Rendering/ViewMath.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/UI/Text/FontAtlas.h"
#include "Core/EditorContext.h"

#include <cstdio>

namespace Editor {

using namespace Aquila;
using SceneManagement::Entity;
using SceneManagement::Components::TransformComponent;

namespace {

constexpr F32 K_AXIS_WIDGET_RADIUS = 34.F;
constexpr F32 K_AXIS_WIDGET_MARGIN = 16.F;
constexpr F32 K_AXIS_BUBBLE = 8.F;
constexpr F32 K_ACTION_SAFE = 0.05F;
constexpr F32 K_TITLE_SAFE = 0.10F;

constexpr std::array<Vec4, 3> K_AXIS_COLORS = {
	Vec4(0.86F, 0.36F, 0.36F, 1.F),
	Vec4(0.42F, 0.74F, 0.40F, 1.F),
	Vec4(0.33F, 0.55F, 0.90F, 1.F),
};
constexpr std::array<const char *, 3> K_AXIS_NAMES = { "X", "Y", "Z" };
constexpr Usize K_MAX_LISTED_PASSES = 10;

std::string milliseconds(F32 value) {
	std::array<char, 32> buffer{};
	std::snprintf(buffer.data(), buffer.size(), "%.2f ms", value);
	return buffer.data();
}

} // namespace

ViewportOverlay::ViewportOverlay(EditorContext &context) : ViewportCanvas(9), m_context(context) {
	add_class("viewport-overlay");
	set_skip_hit_test(true);
}

void ViewportOverlay::sync(const Rect &viewport, const Rendering::RenderView &view) {
	fit(viewport);

	std::string label;
	Option<Vec2> label_anchor;
	Entity selected = m_context.selection.get();
	if (m_context.selection.has() && selected.has_component<TransformComponent>()) {
		const Vec3 origin = Vec3(selected.get_component<TransformComponent>().get_world_matrix_lazy()[3]);
		label_anchor = Rendering::project_to_screen(view, viewport, origin);
		label = selected.get_name();
	}

	if (viewport == m_viewport && view == m_view && label == m_label && label_anchor == m_label_anchor) {
		return;
	}
	m_viewport = viewport;
	m_view = view;
	m_label = std::move(label);
	m_label_anchor = label_anchor;
	redraw();
}

void ViewportOverlay::set_option(bool Options::*flag, bool enabled) {
	if (m_options.*flag == enabled) {
		return;
	}
	m_options.*flag = enabled;
	redraw();
}

void ViewportOverlay::set_debug_info(DebugInfo info) {
	m_debug = std::move(info);
	redraw();
}

void ViewportOverlay::draw(UI::Rendering::DrawList &draw_list) const {
	if (!m_view.valid || m_viewport.size.x < 1.F || m_viewport.size.y < 1.F) {
		return;
	}
	if (wants_debug_info()) {
		draw_debug_panel(draw_list);
	}
	if (m_options.safe_frame) {
		draw_safe_frame(draw_list);
	}
	if (m_options.selection_label) {
		draw_selection_label(draw_list);
	}
	if (m_options.axis_widget) {
		draw_axis_widget(draw_list);
	}
}

void ViewportOverlay::draw_centered_text(UI::Rendering::DrawList &draw_list, Vec2 center, const std::string &text,
										 F32 size, Vec4 color, Int32 z) const {
	UI::Text::FontAtlas *atlas = font();
	if (atlas == nullptr) {
		return;
	}
	atlas->ensure_glyphs(text);
	const Vec2 extent = atlas->measure_text(text, size);
	const Rect bounds = { .position = Math::round(center - (extent * 0.5F)), .size = extent };
	draw_list.draw_text(bounds, text, atlas, color, size, UI::TextAlign::Left, z);
}

void ViewportOverlay::draw_axis_widget(UI::Rendering::DrawList &draw_list) const {
	const Vec2 center = { m_viewport.right() - K_AXIS_WIDGET_MARGIN - K_AXIS_WIDGET_RADIUS - K_AXIS_BUBBLE,
						  m_viewport.top() + K_AXIS_WIDGET_MARGIN + K_AXIS_WIDGET_RADIUS + K_AXIS_BUBBLE };

	struct Bubble {
		Usize axis;
		bool positive;
		Vec2 position;
		F32 depth;
	};
	std::array<Bubble, 6> bubbles{};
	for (Usize axis = 0; axis < 3; ++axis) {
		for (int side = 0; side < 2; ++side) {
			Vec3 direction(0.F);
			direction[static_cast<int>(axis)] = side == 0 ? 1.F : -1.F;
			const Vec2 offset = Vec2(Math::dot(direction, m_view.right), -Math::dot(direction, m_view.up));
			bubbles[(axis * 2) + static_cast<Usize>(side)] = {
				.axis = axis,
				.positive = side == 0,
				.position = center + (offset * K_AXIS_WIDGET_RADIUS),
				.depth = Math::dot(direction, m_view.forward),
			};
		}
	}
	std::ranges::sort(bubbles, [](const Bubble &a, const Bubble &b) { return a.depth > b.depth; });

	draw_list.draw_rect({ .position = center - Vec2(K_AXIS_WIDGET_RADIUS + K_AXIS_BUBBLE + 4.F),
						  .size = Vec2((K_AXIS_WIDGET_RADIUS + K_AXIS_BUBBLE + 4.F) * 2.F) },
						Vec4(1.F, 1.F, 1.F, 0.04F), Vec4(K_AXIS_WIDGET_RADIUS + K_AXIS_BUBBLE + 4.F), 0.F, Vec4(0.F),
						0);

	for (const Bubble &bubble : bubbles) {
		const Vec4 color = K_AXIS_COLORS[bubble.axis];
		const Rect rect = { .position = bubble.position - Vec2(K_AXIS_BUBBLE), .size = Vec2(K_AXIS_BUBBLE * 2.F) };
		if (bubble.positive) {
			draw_list.draw_line(center, bubble.position, 2.F, color, 1);
			draw_list.draw_rect(rect, color, Vec4(K_AXIS_BUBBLE), 0.F, Vec4(0.F), 2);
			draw_centered_text(draw_list, bubble.position, K_AXIS_NAMES[bubble.axis], 11.F,
							   Vec4(0.08F, 0.08F, 0.08F, 1.F), 3);
		} else {
			draw_list.draw_rect(rect, Vec4(Vec3(color), 0.25F), Vec4(K_AXIS_BUBBLE), 1.F, Vec4(Vec3(color), 0.7F), 2);
		}
	}
}

void ViewportOverlay::draw_selection_label(UI::Rendering::DrawList &draw_list) const {
	UI::Text::FontAtlas *atlas = font();
	if (atlas == nullptr || m_label.empty() || !m_label_anchor || !m_viewport.contains(*m_label_anchor)) {
		return;
	}
	const F32 size = font_size();
	atlas->ensure_glyphs(m_label);
	const Vec2 extent = atlas->measure_text(m_label, size);
	const Vec2 padding(6.F, 2.F);
	const Rect box = { .position = Math::round(*m_label_anchor + Vec2(14.F, -30.F)), .size = extent + (padding * 2.F) };
	draw_list.draw_rect(box, Vec4(0.06F, 0.06F, 0.06F, 0.75F), Vec4(3.F), 0.F, Vec4(0.F), 1);
	draw_list.draw_text({ .position = box.position + padding, .size = extent }, m_label, atlas,
						Vec4(0.93F, 0.93F, 0.93F, 1.F), size, UI::TextAlign::Left, 2);
}

void ViewportOverlay::draw_safe_frame(UI::Rendering::DrawList &draw_list) const {
	auto inset = [&](F32 fraction) {
		const Vec2 margin = m_viewport.size * fraction;
		return Rect{ .position = Math::round(m_viewport.position + margin),
					 .size = Math::round(m_viewport.size - (margin * 2.F)) };
	};
	draw_list.draw_rect(inset(K_ACTION_SAFE), Vec4(0.F), Vec4(0.F), 1.F, Vec4(1.F, 1.F, 1.F, 0.35F), 0);
	draw_list.draw_rect(inset(K_TITLE_SAFE), Vec4(0.F), Vec4(0.F), 1.F, Vec4(1.F, 1.F, 1.F, 0.2F), 0);

	const Vec2 center = Math::round(m_viewport.center());
	const Vec4 cross = Vec4(1.F, 1.F, 1.F, 0.35F);
	draw_list.draw_line(center - Vec2(8.F, 0.F), center + Vec2(8.F, 0.F), 1.F, cross, 0);
	draw_list.draw_line(center - Vec2(0.F, 8.F), center + Vec2(0.F, 8.F), 1.F, cross, 0);
}

void ViewportOverlay::draw_debug_panel(UI::Rendering::DrawList &draw_list) const {
	UI::Text::FontAtlas *atlas = font();
	if (atlas == nullptr) {
		return;
	}

	struct Row {
		std::string left{};
		std::string right{};
		F32 bar = -1.F;
		bool header = false;
	};
	std::vector<Row> rows;

	if (m_options.render_status) {
		rows.push_back({ .left = "Render", .header = true });
		rows.push_back({ .left = "Shading", .right = m_debug.shading });
		rows.push_back(
			{ .left = "Resolution", .right = std::to_string(m_debug.width) + " x " + std::to_string(m_debug.height) });
		rows.push_back({ .left = "CPU frame", .right = milliseconds(m_debug.cpu_ms) });
		rows.push_back({ .left = "GPU frame", .right = m_debug.has_gpu ? milliseconds(m_debug.gpu_ms) : "n/a" });
	}

	if (m_options.statistics) {
		const auto &stats = m_debug.statistics;
		rows.push_back({ .left = "Scene", .header = true });
		rows.push_back({ .left = "Objects", .right = Foundation::with_separators(stats.objects) });
		rows.push_back({ .left = "Meshes", .right = Foundation::with_separators(stats.meshes) });
		rows.push_back({ .left = "Lights", .right = Foundation::with_separators(stats.lights) });
		rows.push_back({ .left = "Vertices", .right = Foundation::with_separators(stats.total.vertices) });
		rows.push_back({ .left = "Triangles", .right = Foundation::with_separators(stats.total.triangles) });
	}

	if (m_options.gpu_timings) {
		rows.push_back({ .left = "GPU passes", .header = true });
		if (m_debug.passes.empty()) {
			rows.push_back({ .left = m_debug.has_gpu ? "Waiting for results" : "Timestamps unsupported" });
		} else {
			std::vector<Rendering::RenderPipeline::PassTiming> passes = m_debug.passes;
			std::ranges::sort(passes, [](const auto &a, const auto &b) { return a.milliseconds > b.milliseconds; });
			const F32 slowest = Math::max(passes.front().milliseconds, 0.001F);
			for (Usize i = 0; i < std::min(passes.size(), K_MAX_LISTED_PASSES); ++i) {
				rows.push_back({ .left = passes[i].name,
								 .right = milliseconds(passes[i].milliseconds),
								 .bar = passes[i].milliseconds / slowest });
			}
		}
	}

	if (rows.empty()) {
		return;
	}

	const F32 size = font_size();
	const F32 line = Math::round((atlas->get_line_height() * size) + 4.F);
	const Vec2 padding(10.F, 8.F);
	F32 width = 220.F;
	for (const Row &row : rows) {
		atlas->ensure_glyphs(row.left);
		atlas->ensure_glyphs(row.right);
		width = Math::max(width, atlas->measure_text(row.left, size).x + atlas->measure_text(row.right, size).x + 32.F);
	}
	width += padding.x * 2.F;
	const F32 height = (line * static_cast<F32>(rows.size())) + (padding.y * 2.F);
	const Rect panel = { .position = Math::round(Vec2(m_viewport.left() + 10.F, m_viewport.bottom() - 10.F - height)),
						 .size = Vec2(width, height) };
	draw_list.draw_rect(panel, Vec4(0.06F, 0.06F, 0.06F, 0.78F), Vec4(5.F), 1.F, Vec4(1.F, 1.F, 1.F, 0.06F), 4);

	const Vec4 muted(0.55F, 0.55F, 0.55F, 1.F);
	const Vec4 dim(0.72F, 0.72F, 0.72F, 1.F);
	const Vec4 bright(0.94F, 0.94F, 0.94F, 1.F);
	const Vec4 bar_color(0.29F, 0.62F, 0.37F, 0.9F);
	const F32 inner_width = width - (padding.x * 2.F);
	F32 y = panel.position.y + padding.y;
	for (const Row &row : rows) {
		const Rect bounds = { .position = { panel.position.x + padding.x, y }, .size = { inner_width, line } };
		draw_list.draw_text(bounds, row.left, atlas, row.header ? muted : dim, size, UI::TextAlign::Left, 5);
		if (!row.right.empty()) {
			draw_list.draw_text(bounds, row.right, atlas, bright, size, UI::TextAlign::Right, 5);
		}
		if (row.bar >= 0.F) {
			draw_list.draw_rect({ .position = { bounds.position.x, y + line - 3.F },
								  .size = { Math::max(inner_width * row.bar, 1.F), 2.F } },
								bar_color, Vec4(1.F), 0.F, Vec4(0.F), 5);
		}
		y += line;
	}
}

} // namespace Editor
