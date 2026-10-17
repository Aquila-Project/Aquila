#pragma once

#include "Aquila/UI/Widgets/Control.h"
#include "Aquila/UI/Widgets/IconLabel.h"
#include "Aquila/UI/Style/StyleProperties.h"

namespace Aquila::UI::Core {

/// In markup, on-click names a command registered on the LayoutLoader:
/// `<Button on-click="file.save">Save</Button>`.
class Button : public Control {
  public:
	Button();
	Button(std::string text, Text::FontAtlas *font = nullptr);

	[[nodiscard]] std::string_view get_type_name() const override { return "Button"; }

	void set_text(std::string text);
	void set_font(Text::FontAtlas *font) override;
	void set_icon(GFX::GfxTexture *texture);
	/// Text on the right side, like a menu item's key.
	void set_shortcut(std::string shortcut);
	/// Keeps the icon slot even without an icon, so menu items line up.
	void set_reserve_icon_space(bool reserve);
	void set_trailing_icon(GFX::GfxTexture *texture);
	[[nodiscard]] IconLabel *get_content() const { return m_content; }
	Signal<void()> on_click;

	void on_mouse_release(Platform::MouseButton btn, Vec2 pos) override;
	void on_style_resolved() override;
	void apply_xml_text_content(std::string_view text) override { set_text(std::string(text)); }
	void apply_xml_attribute(std::string_view name, std::string_view value,
							 IResourceResolver *resolver = nullptr) override;

  private:
	void ensure_content();

	IconLabel *m_content = nullptr;
};

} // namespace Aquila::UI::Core
