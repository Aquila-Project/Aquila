#include "Aquila/UI/Rendering/DrawList.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Foundation/Text/Utf8.h"
#include "Aquila/Graphics/Core/Renderer2D.h"
#include "Aquila/UI/Core/TextureCache.h"
#include "Aquila/UI/Rendering/DrawCmd.h"
#include "Aquila/UI/Text/FontAtlas.h"

namespace Aquila::UI::Rendering {

namespace {

constexpr F32 k_glyph_dilation_px = 1.F;
constexpr F32 k_wrap_tolerance_px = 0.5F;

}

void DrawList::draw_rect(Rect rect, Vec4 color, Vec4 radius, F32 border_width, Vec4 border_color, Int32 z,
						 BorderStyle border_style) {
	RectCmd command;
	command.rect = rect;
	command.color = color;
	command.radius = radius;
	command.border_width = border_width;
	command.border_color = border_color;
	command.border_style = border_style;
	command.z_order = z;

	m_commands.emplace_back(command);
}

void DrawList::draw_line(Vec2 from, Vec2 to, float width, Vec4 color, Int32 z) {
	Vec2 delta = to - from;
	float length = Math::length(delta);
	if (length < 0.5F) {
		return;
	}

	Vec2 center = (from + to) * 0.5F;
	float angle = Math::atan2(delta.y, delta.x);

	RectCmd command;
	command.rect = { .position = center - Vec2(length * 0.5F, width * 0.5F), .size = { length, width } };
	command.color = color;
	command.rotation = angle;
	command.z_order = z;

	m_commands.emplace_back(command);
}

void DrawList::draw_point(Vec2 position, float width, Vec4 color, Int32 z) {
	PointCmd command{};
	command.position = position;
	command.color = color;
	command.size = width;
	command.z_order = z;

	m_commands.emplace_back(command);
}

void DrawList::draw_shadow(Rect widget_rect, Vec2 offset, float blur, float spread, Vec4 color, Vec4 radius, Int32 z) {
	ShadowCmd command;
	command.rect = widget_rect;
	command.color = color;
	command.radius = radius;
	command.offset = offset;
	command.original_half_size = { (widget_rect.size.x * 0.5F) + spread, (widget_rect.size.y * 0.5F) + spread };
	command.blur = blur;
	command.z_order = z;

	m_commands.emplace_back(command);
}

void DrawList::draw_text(Rect bounds, std::string_view text, Text::FontAtlas *font, Vec4 color, float font_size,
						 TextAlign align, Int32 z, bool wrap) {
	if ((font == nullptr) || text.empty()) {
		return;
	}

	font->ensure_glyphs(text);

	TextCmd command;
	command.rect = bounds;
	command.color = color;
	command.z_order = z;
	command.text = std::string(text);
	command.font = font;
	command.font_size = font_size;
	command.align = align;
	command.wrap = wrap;
	m_commands.emplace_back(std::move(command));
}

void DrawList::draw_image(Rect rect, GFX::GfxTexture *tex, Vec4 tint, Vec2 uv_min, Vec2 uv_max, Int32 z) {
	const Vec2 snapped_size = Math::round(rect.position + rect.size) - Math::round(rect.position);
	ImageCmd command;
	command.rect = rect;
	command.texture = Core::TextureCache::resolve_for_size(tex, snapped_size);
	command.z_order = z;
	command.tint = tint;
	command.uv_min = uv_min;
	command.uv_max = uv_max;

	m_commands.emplace_back(command);
}

void DrawList::push_clip(Rect clip_rect) {
	m_clip_stack.push_back(clip_rect);
	ClipPushCmd cmd;
	cmd.rect = clip_rect;
	m_commands.emplace_back(cmd);
}

void DrawList::pop_clip() {
	if (!m_clip_stack.empty()) {
		m_clip_stack.pop_back();
	}

	ClipPopCmd cmd;
	cmd.rect = m_clip_stack.empty() ? Rect{} : m_clip_stack.back();
	m_commands.emplace_back(cmd);
}

void DrawList::sort() {
	std::ranges::stable_sort(m_commands, {}, [](const DrawCmd &c) { return draw_cmd_z_order(c); });
}

void DrawList::submit(Graphics::Renderer2D &r2d, GFX::GfxCommandList &cmd) {
	for (auto &command : m_commands) {
		std::visit(
			[&](auto &c) {
				using T = std::decay_t<decltype(c)>;

				if constexpr (std::is_same_v<T, RectCmd>) {
					Graphics::RectSpec spec{};
					spec.position = c.rect.position;
					spec.size = c.rect.size;
					spec.color = c.color;
					spec.radius = c.radius;
					spec.border_width = c.border_width;
					spec.border_color = c.border_color;
					spec.border_style = static_cast<float>(static_cast<Uint8>(c.border_style));
					spec.rotation = c.rotation;
					r2d.draw_rect(spec);
				} else if constexpr (std::is_same_v<T, PointCmd>) {
					Graphics::PointSpec spec;
					spec.position = c.position;
					spec.color = c.color;
					spec.size = c.size;
					r2d.draw_point(spec);
				} else if constexpr (std::is_same_v<T, ShadowCmd>) {
					Graphics::ShadowSpec spec{};
					const Vec2 offset = c.offset;
					const Vec2 sdf_half_size = c.original_half_size;
					const float blur = c.blur;
					spec.position = c.rect.position + offset - Vec2(blur + (sdf_half_size.x - c.rect.size.x * 0.5f));
					spec.size = c.rect.size +
						Vec2(2.F * (blur + (sdf_half_size.x - c.rect.size.x * 0.5f)),
							 2.F * (blur + (sdf_half_size.y - c.rect.size.y * 0.5f)));
					spec.color = c.color;
					spec.offset = offset;
					spec.original_half_size = sdf_half_size;
					spec.radius = c.radius;
					spec.blur = blur;
					r2d.draw_shadow(spec);
				} else if constexpr (std::is_same_v<T, ImageCmd>) {
					Graphics::SpriteSpec spec{};
					spec.position = Math::round(c.rect.position);
					spec.size = Math::round(c.rect.position + c.rect.size) - spec.position;
					spec.tint = c.tint;
					spec.texture = c.texture;
					spec.uv_min = c.uv_min;
					spec.uv_max = c.uv_max;
					r2d.draw_sprite(spec);
				} else if constexpr (std::is_same_v<T, TextCmd>) {
					if ((c.font == nullptr) || c.text.empty()) {
						return;
					}

					Text::FontAtlas *atlas = c.font;
					const F32 depth = 0.F;

					const F32 render_size = (c.font_size > 0.F) ? c.font_size : SharedConstants::FONT_DEFAULT_SIZE;
					const F32 scale = render_size;
					const F32 line_height = c.font->get_line_height() * scale;
					const F32 ascent = c.font->get_ascent() * scale;

					GFX::GfxTexture *curve_texture = atlas->get_curve_texture();
					GFX::GfxTexture *band_texture = atlas->get_band_texture();

					auto draw_line_range = [&](size_t start, size_t end, F32 baseline_y) {
						F32 text_width = 0.F;
						for (size_t ci = start; ci < end;) {
							const Foundation::Utf8::Decoded d = Foundation::Utf8::decode(c.text, ci);
							ci += (d.size > 0 ? d.size : 1u);
							if (const Text::GlyphInfo *g = atlas->get_glyph(d.codepoint)) {
								text_width += g->advance_em * scale;
							}
						}

						baseline_y = Math::round(baseline_y);
						F32 cursor_x = c.rect.position.x;
						if (c.align == TextAlign::Center) {
							cursor_x += (c.rect.size.x - text_width) * 0.5F;
						} else if (c.align == TextAlign::Right) {
							cursor_x += c.rect.size.x - text_width;
						} else {
							for (size_t ci = start; ci < end;) {
								const Foundation::Utf8::Decoded d = Foundation::Utf8::decode(c.text, ci);
								ci += (d.size > 0 ? d.size : 1u);
								if (const Text::GlyphInfo *g = atlas->get_glyph(d.codepoint)) {
									cursor_x -= g->bearing_em.x * scale;
									break;
								}
							}
						}
						cursor_x = Math::round(cursor_x);

						for (size_t ci = start; ci < end;) {
							const Foundation::Utf8::Decoded d = Foundation::Utf8::decode(c.text, ci);
							ci += (d.size > 0 ? d.size : 1u);
							const Text::GlyphInfo *glyph = atlas->get_glyph(d.codepoint);
							if (glyph == nullptr) {
								continue;
							}
							const Text::SlugGlyphData *slug = atlas->get_slug_data(glyph->glyph_id);
							const Vec2 glyph_size = glyph->size_em * scale;
							if (slug != nullptr && glyph_size.x > 0.F && glyph_size.y > 0.F) {
								const F32 glyph_x = cursor_x + glyph->bearing_em.x * scale;
								const F32 glyph_y = baseline_y + glyph->bearing_em.y * scale;
								const Vec2 em_per_pixel = (slug->em_max - slug->em_min) / glyph_size;

								Graphics::GlyphSpec spec{};
								spec.position = Vec2(glyph_x, glyph_y) - Vec2(k_glyph_dilation_px);
								spec.size = glyph_size + Vec2(2.F * k_glyph_dilation_px);
								spec.color = c.color;
								spec.depth = depth;
								spec.glyph_loc_x = slug->glyph_loc_x;
								spec.glyph_loc_y = slug->glyph_loc_y;
								spec.band_max_x = slug->band_max_x;
								spec.band_max_y = slug->band_max_y;
								spec.banding = slug->band_transform;
								spec.em_min = slug->em_min - (em_per_pixel * k_glyph_dilation_px);
								spec.em_max = slug->em_max + (em_per_pixel * k_glyph_dilation_px);
								spec.curve_texture = curve_texture;
								spec.band_texture = band_texture;
								r2d.draw_glyph(spec);
							}
							cursor_x += glyph->advance_em * scale;
						}
					};

					if (!c.wrap) {
						draw_line_range(0, c.text.size(), c.rect.position.y + ascent);
					} else {
						const std::string &s = c.text;
						const F32 max_w = c.rect.size.x;
						const F32 space_w = atlas->measure_text(" ", render_size).x;
						F32 baseline_y = c.rect.position.y + ascent;
						size_t line_start = std::string::npos;
						size_t line_end = 0;
						F32 line_w = 0.F;

						auto flush = [&]() {
							if (line_start != std::string::npos) {
								draw_line_range(line_start, line_end, baseline_y);
							}
							baseline_y += line_height;
							line_start = std::string::npos;
							line_w = 0.F;
						};

						size_t i = 0;
						while (i < s.size()) {
							if (s[i] == '\n') {
								flush();
								++i;
								continue;
							}
							if (s[i] == ' ') {
								++i;
								continue;
							}
							const size_t word_start = i;
							while (i < s.size() && s[i] != ' ' && s[i] != '\n') {
								++i;
							}
							const size_t word_end = i;
							const F32 word_w =
								atlas
									->measure_text(std::string_view(s).substr(word_start, word_end - word_start),
												   render_size)
									.x;
							if (line_start == std::string::npos) {
								line_start = word_start;
								line_end = word_end;
								line_w = word_w;
							} else if (line_w + space_w + word_w <= max_w + k_wrap_tolerance_px) {
								line_end = word_end;
								line_w += space_w + word_w;
							} else {
								flush();
								line_start = word_start;
								line_end = word_end;
								line_w = word_w;
							}
						}
						flush();
					}
				} else if constexpr (std::is_same_v<T, ClipPushCmd>) {
					r2d.flush();
					r2d.set_scissor(cmd, static_cast<Int32>(c.rect.left()), static_cast<Int32>(c.rect.top()),
									static_cast<Uint32>(c.rect.width()), static_cast<Uint32>(c.rect.height()));
				} else if constexpr (std::is_same_v<T, ClipPopCmd>) {
					r2d.flush();
					if (c.rect.is_empty()) {
						r2d.set_scissor(cmd, 0, 0, m_canvas_width, m_canvas_height);
					} else {
						r2d.set_scissor(cmd, static_cast<Int32>(c.rect.left()), static_cast<Int32>(c.rect.top()),
										static_cast<Uint32>(c.rect.width()), static_cast<Uint32>(c.rect.height()));
					}
				}
			},
			command);
	}
}

void DrawList::append_cmd(const DrawCmd &cmd) {
	m_commands.push_back(cmd);
}

std::vector<DrawCmd> DrawList::take_commands() {
	m_clip_stack.clear();
	return std::move(m_commands);
}

void DrawList::clear() {
	m_clip_stack.clear();
	m_commands.clear();
}

bool DrawList::is_empty() const {
	return m_commands.empty();
}

void DrawList::set_canvas_size(Uint32 width, Uint32 height) {
	m_canvas_width = width;
	m_canvas_height = height;
}

Option<Rect> DrawList::active_clip() const {
	if (m_clip_stack.empty()) {
		return std::nullopt;
	}
	return m_clip_stack.back();
}
} // namespace Aquila::UI::Rendering
