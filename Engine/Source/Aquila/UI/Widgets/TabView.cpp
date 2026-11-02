#include "Aquila/UI/Widgets/TabView.h"

#include "Aquila/UI/Core/MarkupItem.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/TabBar.h"

namespace Aquila::UI::Core {

TabView::TabView() {
	m_tab_bar = add_child<TabBar>("tab-button");
	m_tab_bar->add_class("tab-bar");
	m_tab_bar->on_selected.connect([this](int index) { set_active_tab(index); });

	m_panels = add_child<View>();
	m_panels->add_class("tab-panels");
}

View *TabView::add_tab(std::string title, GFX::GfxTexture *icon) {
	m_tab_bar->add_tab(std::move(title), icon);

	auto *panel = m_panels->add_child<View>();
	panel->add_class("tab-panel");
	m_tab_panels.push_back(panel);

	if (m_active_tab < 0) {
		set_active_tab(0);
	} else {
		apply_active_tab();
	}

	return panel;
}

bool TabView::add_markup_item(MarkupItem &item, IResourceResolver *resolver) {
	if (item.tag != "Tab") {
		return false;
	}
	View *panel = add_tab(std::string(item.get("title")), item.get_texture("icon", resolver));
	if (const std::string_view tooltip = item.get("tooltip"); !tooltip.empty()) {
		m_tab_bar->get_tab(m_tab_bar->get_tab_count() - 1)->set_tooltip(std::string(tooltip));
	}
	for (Unique<View> &view : item.views) {
		panel->add_child(std::move(view));
	}
	item.views.clear();
	return true;
}

void TabView::set_active_tab(int index) {
	if (index < 0 || index >= static_cast<int>(m_tab_panels.size())) {
		return;
	}
	m_active_tab = index;
	apply_active_tab();
	on_tab_changed(m_active_tab);
}

void TabView::apply_active_tab() {
	m_tab_bar->set_active(m_active_tab);
	for (int i = 0; i < static_cast<int>(m_tab_panels.size()); ++i) {
		m_tab_panels[static_cast<Usize>(i)]->set_hidden(i != m_active_tab);
	}
}

}
