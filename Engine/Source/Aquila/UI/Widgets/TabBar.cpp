#include "Aquila/UI/Widgets/TabBar.h"

#include "Aquila/UI/Core/MarkupItem.h"
#include "Aquila/UI/Widgets/Button.h"

namespace Aquila::UI::Core {

TabBar::TabBar() : TabBar("tab-button") {}

TabBar::TabBar(std::string tab_class) : m_tab_class(std::move(tab_class)) {}

Button *TabBar::add_tab(std::string label, GFX::GfxTexture *icon) {
	auto *tab = add_child<Button>(std::move(label));
	if (icon != nullptr) {
		tab->set_icon(icon);
	}
	tab->add_class(m_tab_class);
	const int index = static_cast<int>(m_tabs.size());
	tab->on_click.connect([this, index] {
		set_active(index);
		on_selected(index);
	});
	m_tabs.push_back(tab);
	return tab;
}

void TabBar::set_active(int index) {
	m_active = index >= 0 && index < get_tab_count() ? index : -1;
	apply_classes();
}

Button *TabBar::get_tab(int index) const {
	return index >= 0 && index < get_tab_count() ? m_tabs[static_cast<Usize>(index)] : nullptr;
}

void TabBar::set_tab_class(std::string tab_class) {
	for (Button *tab : m_tabs) {
		tab->remove_class(m_tab_class);
		tab->remove_class(m_tab_class + "-active");
		tab->add_class(tab_class);
	}
	m_tab_class = std::move(tab_class);
	apply_classes();
}

void TabBar::apply_xml_attribute(std::string_view name, std::string_view value, IResourceResolver *resolver) {
	if (name == "tab-class") {
		set_tab_class(std::string(value));
		return;
	}
	View::apply_xml_attribute(name, value, resolver);
}

bool TabBar::add_markup_item(MarkupItem &item, IResourceResolver *resolver) {
	if (item.tag != "Tab") {
		return false;
	}
	Button *tab = add_tab(std::string(item.get("title")), item.get_texture("icon", resolver));
	if (const std::string_view tooltip = item.get("tooltip"); !tooltip.empty()) {
		tab->set_tooltip(std::string(tooltip));
	}
	if (!item.views.empty()) {
		AQUILA_LOG_WARNING("TabBar: {}: a TabBar's tabs have no page, the elements inside <Tab> are left out",
						   item.location);
	}
	return true;
}

void TabBar::apply_classes() {
	const std::string active_class = m_tab_class + "-active";
	for (int i = 0; i < get_tab_count(); ++i) {
		m_tabs[static_cast<Usize>(i)]->set_class(active_class, i == m_active);
	}
}

}
