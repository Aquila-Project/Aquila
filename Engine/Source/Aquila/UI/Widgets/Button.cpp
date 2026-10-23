#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Core/IResourceResolver.h"
#include "Aquila/Foundation/Macros.h"

namespace Aquila::UI::Core {

Button::Button() = default;

Button::Button(std::string text, Text::FontAtlas *font) {
	m_content = add_child<IconLabel>(std::move(text), font);
}

void Button::ensure_content() {
	if (m_content == nullptr) {
		m_content = add_child<IconLabel>();
	}
}

void Button::set_text(std::string text) {
	ensure_content();
	m_content->set_text(std::move(text));
}

void Button::set_shortcut(std::string shortcut) {
	ensure_content();
	m_content->set_shortcut(std::move(shortcut));
}

void Button::set_font(Text::FontAtlas *font) {
	if (m_content == nullptr) {
		return;
	}
	m_content->set_font(font);
}

void Button::set_icon(GFX::GfxTexture *texture) {
	ensure_content();
	m_content->set_icon_texture(texture);
}

void Button::set_reserve_icon_space(bool reserve) {
	ensure_content();
	m_content->set_reserve_icon_space(reserve);
}

void Button::set_trailing_icon(GFX::GfxTexture *texture) {
	ensure_content();
	m_content->set_trailing_icon(texture);
}

void Button::on_style_resolved() {
	View::on_style_resolved();
	if (m_content == nullptr) {
		return;
	}
	if (Text::FontAtlas *font = get_resolved_font()) {
		m_content->set_font(font);
	}
}

void Button::apply_xml_attribute(std::string_view name, std::string_view value, IResourceResolver *resolver) {
	if (name == "src" || name == "uv" || name == "tint") {
		ensure_content();
		m_content->apply_xml_attribute(name, value, resolver);
		return;
	}
	if (name == "on-click") {
		if (resolver != nullptr) {
			if (Delegate<void()> command = resolver->resolve_command(std::string(value))) {
				on_click.connect(std::move(command));
			} else {
				AQUILA_LOG_WARNING("Button: on-click references unknown command '{}'", value);
			}
		}
		return;
	}
	View::apply_xml_attribute(name, value, resolver);
}

} // namespace Aquila::UI::Core
