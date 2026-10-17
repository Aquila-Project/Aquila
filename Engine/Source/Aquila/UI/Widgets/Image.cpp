#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/UI/Core/IResourceResolver.h"
#include "Aquila/UI/Style/StyleParserHelper.h"

namespace Aquila::UI::Core {

Image::Image() {
	m_should_skip_hit_test = true;
}

Image::Image(GFX::GfxTexture *texture, Vec4 tint) : m_texture(texture), m_tint(tint) {
	m_should_skip_hit_test = true;
}

void Image::set_texture(GFX::GfxTexture *texture) {
	if (texture == m_texture) {
		return;
	}
	m_texture = texture;
	invalidate_layout();
}

void Image::set_tint(Vec4 tint) {
	if (tint == m_tint) {
		return;
	}
	m_tint = tint;
	queue_redraw();
}

void Image::set_uv_region(Vec2 uv_min, Vec2 uv_max) {
	if (uv_min == m_uv_min && uv_max == m_uv_max) {
		return;
	}
	m_uv_min = uv_min;
	m_uv_max = uv_max;
	invalidate_layout();
}

void Image::on_draw_self(Rendering::DrawList &draw_list) {
	View::on_draw_self(draw_list);

	if (m_texture == nullptr) {
		return;
	}

	const Rect world_rect = get_absolute_rect();
	const Vec4 tint = m_tint * get_display_style().color;
	draw_list.draw_image(world_rect, m_texture, tint, m_uv_min, m_uv_max, 2);
}

void Image::apply_xml_attribute(std::string_view name, std::string_view value, IResourceResolver *resolver) {
	if (name == "tint") {
		if (auto c = UI::ParserHelper::parse_color(value)) {
			set_tint(*c);
		}
		return;
	}
	if (name == "src") {
		if (resolver != nullptr) {
			if (GFX::GfxTexture *tex = resolver->resolve_texture(std::string(value))) {
				set_texture(tex);
			} else {
				AQUILA_LOG_WARNING("Image: could not load texture '{}'", value);
			}
		}
		return;
	}
	if (name == "uv") {
		float u0 = 0.F, v0 = 0.F, u1 = 1.F, v1 = 1.F;
		std::istringstream ss{ std::string(value) };
		if (ss >> u0 >> v0 >> u1 >> v1) {
			set_uv_region({ u0, v0 }, { u1, v1 });
		}
		return;
	}
	View::apply_xml_attribute(name, value, resolver);
}

} // namespace Aquila::UI::Core
