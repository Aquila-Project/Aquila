#include "Aquila/UI/Widgets/TabView.h"

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
