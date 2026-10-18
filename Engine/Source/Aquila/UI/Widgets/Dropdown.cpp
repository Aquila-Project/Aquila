#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Core/IResourceResolver.h"

namespace Aquila::UI::Core {

namespace {

constexpr F32 k_sizer_height = 0.001F;

}

Dropdown::Dropdown() {
	auto header = std::make_unique<Button>();
	header->add_class("dropdown-header");
	header->on_click.connect([this] { toggle_popup(); });
	m_header = dynamic_cast<Button *>(add_child(std::move(header)));

	auto popup = std::make_unique<Popup>();
	popup->add_class("dropdown-popup");
	m_popup = dynamic_cast<Popup *>(add_child(std::move(popup)));

	update_header();

	StyleProperties header_style;
	header_style.flex_direction = FlexDirection::Column;
	header_style.align_items = AlignItems::Stretch;
	header_style.justify_content = JustifyContent::Center;
	header_style.gap = 0.F;
	m_header->merge_style(header_style);

	m_sizer = m_header->add_child<View>();
	m_sizer->add_class("dropdown-sizer");
	StyleProperties sizer_style;
	sizer_style.flex_direction = FlexDirection::Column;
	sizer_style.height = StyleLength::pixel(k_sizer_height);
	sizer_style.overflow = Overflow::Clip;
	sizer_style.opacity = 0.F;
	m_sizer->merge_style(sizer_style);
	rebuild_sizer();
}

void Dropdown::add_option(std::string value, std::string display, GFX::GfxTexture *icon) {
	m_options.push_back({ .value = std::move(value), .display = std::move(display), .icon = icon });
	rebuild();
	update_header();
}

void Dropdown::clear_options() {
	m_options.clear();
	m_value.clear();
	rebuild();
	update_header();
}

void Dropdown::set_value(const std::string &value) {
	for (const auto &opt : m_options) {
		if (opt.value == value) {
			m_value = value;
			update_header();
			return;
		}
	}
}

void Dropdown::clear_selection() {
	m_value.clear();
	update_header();
}

void Dropdown::set_icon(GFX::GfxTexture *icon) {
	m_icon = icon;
	update_header();
	rebuild_sizer();
}

void Dropdown::set_chevron(GFX::GfxTexture *chevron) {
	m_chevron = chevron;
	m_header->set_trailing_icon(chevron);
	rebuild_sizer();
}

void Dropdown::set_variant(const std::string &variant) {
	add_class(variant);
	m_header->add_class(variant + "-header");
	m_popup->add_class(variant + "-popup");
}

void Dropdown::apply_xml_attribute(std::string_view name, std::string_view value, IResourceResolver *resolver) {
	if (name == "src") {
		if (resolver != nullptr) {
			set_icon(resolver->resolve_texture(std::string(value)));
		}
		return;
	}
	if (name == "chevron") {
		if (resolver != nullptr) {
			set_chevron(resolver->resolve_texture(std::string(value)));
		}
		return;
	}
	if (name == "variant") {
		set_variant(std::string(value));
		return;
	}
	if (name == "placeholder") {
		set_placeholder(std::string(value));
		return;
	}
	Control::apply_xml_attribute(name, value, resolver);
}

void Dropdown::set_placeholder(std::string text) {
	m_placeholder = std::move(text);
	update_header();
	rebuild_sizer();
}

void Dropdown::rebuild() {
	for (View *v : m_option_buttons) {
		m_popup->remove_child(v);
	}
	m_option_buttons.clear();

	for (const auto &opt : m_options) {
		auto btn = std::make_unique<Button>();
		btn->set_reserve_icon_space(true);
		btn->set_icon(opt.icon);
		btn->set_text(opt.label());
		btn->add_class("dropdown-option");
		btn->on_click.connect([this, value = opt.value] {
			select(value);
			m_popup->close();
		});
		m_option_buttons.push_back(m_popup->add_child(std::move(btn)));
	}
	rebuild_sizer();
}

void Dropdown::rebuild_sizer() {
	if (m_sizer == nullptr) {
		return;
	}
	while (!m_sizer->get_children().empty()) {
		m_sizer->remove_child(m_sizer->get_children().back().get());
	}
	const bool reserve_icon = has_any_icon() || m_icon != nullptr;
	auto add_row = [&](const std::string &text) {
		if (text.empty()) {
			return;
		}
		auto *row = m_sizer->add_child<IconLabel>(text);
		row->set_reserve_icon_space(reserve_icon);
		row->set_trailing_icon(m_chevron);
	};
	add_row(m_placeholder);
	for (const auto &opt : m_options) {
		add_row(opt.label());
	}
}

void Dropdown::select(const std::string &value) {
	m_value = value;
	update_header();
	on_changed(m_value);
}

void Dropdown::toggle_popup() {
	// Match the open list to the trigger's width so the popup never ends up wider
	// (or narrower) than the collapsed control, overriding the shared min-width.
	StyleProperties sp;
	sp.min_width = StyleLength::pixel(m_header->get_layout_rect().size.x);
	m_popup->merge_style(sp);
	m_popup->toggle();
}

const Dropdown::Option *Dropdown::find_option(const std::string &value) const {
	if (value.empty()) {
		return nullptr;
	}
	for (const auto &opt : m_options) {
		if (opt.value == value) {
			return &opt;
		}
	}
	return nullptr;
}

bool Dropdown::has_any_icon() const {
	return std::ranges::any_of(m_options, [](const Option &opt) { return opt.icon != nullptr; });
}

void Dropdown::update_header() {
	const Option *selected = find_option(m_value);
	m_header->set_text(selected != nullptr ? selected->label() : m_placeholder);
	GFX::GfxTexture *icon = selected != nullptr && selected->icon != nullptr ? selected->icon : m_icon;
	m_header->set_icon(icon);
	m_header->set_reserve_icon_space(has_any_icon() || m_icon != nullptr);
}

} // namespace Aquila::UI::Core
