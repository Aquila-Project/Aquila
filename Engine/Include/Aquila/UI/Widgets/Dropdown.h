#pragma once

#include "Aquila/UI/Widgets/Control.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Popup.h"

namespace Aquila::UI::Core {

class Dropdown : public Control {
  public:
	Dropdown();

	[[nodiscard]] std::string_view get_type_name() const override { return "Dropdown"; }

	void add_option(std::string value, std::string display = "", GFX::GfxTexture *icon = nullptr);
	void clear_options();

	void set_value(const std::string &value);
	void clear_selection();
	[[nodiscard]] const std::string &get_value() const { return m_value; }

	void set_placeholder(std::string text);
	void set_icon(GFX::GfxTexture *icon);
	void set_chevron(GFX::GfxTexture *chevron);
	void set_variant(const std::string &variant);
	Signal<void(const std::string &)> on_changed;

	[[nodiscard]] Vec2 get_intrinsic_size() const override;
	void apply_xml_attribute(std::string_view name, std::string_view value,
							 IResourceResolver *resolver = nullptr) override;

  private:
	struct Option {
		std::string value;
		std::string display;
		GFX::GfxTexture *icon = nullptr;
		[[nodiscard]] const std::string &label() const { return display.empty() ? value : display; }
	};

	void rebuild();
	void select(const std::string &value);
	void update_header();
	[[nodiscard]] const Option *find_option(const std::string &value) const;
	[[nodiscard]] bool has_any_icon() const;
	void toggle_popup();

	Button *m_header = nullptr;
	Popup *m_popup = nullptr;
	std::vector<Option> m_options;
	std::string m_value;
	std::string m_placeholder = "Select…";
	GFX::GfxTexture *m_icon = nullptr;
	GFX::GfxTexture *m_chevron = nullptr;
	std::vector<View *> m_option_buttons;
};

} // namespace Aquila::UI::Core
