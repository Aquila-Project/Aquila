#include "Aquila/UI/Widgets/InspectorView.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/UIResources.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Collapsible.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/InspectorPage.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/ReflectedPropertyGrid.h"
#include "Aquila/UI/Widgets/ScrollView.h"
#include "Aquila/UI/Widgets/TextInput.h"

namespace Aquila::UI::Core {

namespace {

constexpr std::string_view K_COMPONENTS = "inspector";

template <std::derived_from<View> T>
T *create_as(LayoutLoader &loader, std::string_view component, View &parent, ComponentArgs args = {}) {
	View *created = loader.create(engine_component_file(K_COMPONENTS), component, parent, std::move(args));
	if (created == nullptr) {
		return nullptr;
	}
	auto *typed = dynamic_cast<T *>(created);
	if (typed == nullptr) {
		AQUILA_LOG_ERROR("InspectorView: {} in {} can't be rooted in a {}", component,
						 engine_component_file(K_COMPONENTS), created->get_type_name());
		parent.remove_child(created);
	}
	return typed;
}

}

InspectorView::InspectorView(View &root, IconResolver icons, GFX::GfxContext *ctx)
	: m_icons(std::move(icons)), m_ctx(ctx) {
	/// `root`: props-tabs, props-head, props-head-icon, inspector-name, inspector-uuid, inspector-empty,
	/// inspector-scroll, inspector-pages.

	auto *tab = root.find_by_id("props-tabs");
	if (tab != nullptr) {
		m_tab_strip = tab;
	} else {
		AQUILA_LOG_ERROR("Missing ID props-tabs");
	}

	auto *head = root.find_by_id("props-head");
	if (head != nullptr) {
		m_head = head;
	} else {
		AQUILA_LOG_ERROR("Missing ID props-head");
	}

	auto *head_icon = root.find_by_id<Image>("props-head-icon");
	if (head_icon != nullptr) {
		m_head_icon = head_icon;
	} else {
		AQUILA_LOG_ERROR("Missing ID props-head-icon");
	}

	auto *name = root.find_by_id<TextInput>("inspector-name");
	if (name != nullptr) {
		m_title = name;
	} else {
		AQUILA_LOG_ERROR("Missing ID inspector-name");
	}

	auto *uuid = root.find_by_id<Label>("inspector-uuid");
	if (uuid != nullptr) {
		m_subtitle = uuid;
	} else {
		AQUILA_LOG_ERROR("Missing ID inspector-uuid");
	}

	auto *empty = root.find_by_id("inspector-empty");
	if (empty != nullptr) {
		m_empty = empty;
	} else {
		AQUILA_LOG_ERROR("Missing ID inspector-empty");
	}

	auto *scroll = root.find_by_id<ScrollView>("inspector-scroll");
	if (scroll != nullptr) {
		m_scroll = scroll;
	} else {
		AQUILA_LOG_ERROR("Missing ID inspector-scroll");
	}

	auto *pages = root.find_by_id("inspector-pages");
	if (pages != nullptr) {
		m_pages = pages;
	} else {
		AQUILA_LOG_ERROR("Missing ID inspector-pages");
	}

	if (!has_layout()) {
		return;
	}
	m_title_connection = m_title->on_submit.connect([this](const std::string &name) {
		if (m_inspectable != nullptr && m_inspectable->rename) {
			const Delegate<void(const std::string &)> rename = m_inspectable->rename;
			rename(name);
		}
	});
	clear();
}

InspectorView::~InspectorView() {
	if (!has_layout()) {
		return;
	}
	m_title->on_submit.disconnect(m_title_connection);
	retire();
	m_retired.clear();
}

bool InspectorView::has_layout() const {
	return m_tab_strip != nullptr && m_pages != nullptr && m_head != nullptr && m_head_icon != nullptr &&
		m_title != nullptr && m_subtitle != nullptr && m_empty != nullptr && m_scroll != nullptr;
}

void InspectorView::show(Inspectable inspectable) {
	if (!has_layout()) {
		return;
	}
	retire();
	m_inspectable = std::make_unique<Inspectable>(std::move(inspectable));

	build_header();
	if (LayoutLoader *loader = find_layout_loader(*m_pages)) {
		for (InspectorTab &tab : m_inspectable->tabs) {
			build_tab(*loader, tab);
		}
	} else {
		AQUILA_LOG_ERROR("InspectorView: '{}' needs a canvas with a layout loader", m_inspectable->title);
	}
	m_tab_strip->set_hidden(m_tabs.size() <= 1);
	refresh_tabs();

	m_empty->set_hidden(true);
	m_head->set_hidden(false);
	m_scroll->set_hidden(false);
}

void InspectorView::clear() {
	if (!has_layout()) {
		return;
	}
	retire();
	m_empty->set_hidden(false);
	m_head->set_hidden(true);
	m_tab_strip->set_hidden(true);
	m_scroll->set_hidden(true);
}

void InspectorView::refresh() {
	for (ShownSection &shown : m_sections) {
		bind(shown);
		if (shown.visible && shown.section->content != nullptr) {
			shown.section->content->refresh();
		}
	}
	refresh_tabs();
}

void InspectorView::update() {
	m_retired.clear();
}

View *InspectorView::find_section(std::string_view id) const {
	const auto it =
		std::ranges::find_if(m_sections, [id](const ShownSection &shown) { return shown.section->id == id; });
	return it == m_sections.end() ? nullptr : it->root;
}

void InspectorView::select_tab(std::string_view title) {
	m_preferred_tab = title;
	refresh_tabs();
}

void InspectorView::retire() {
	Retired retired;
	for (const ShownTab &shown : m_tabs) {
		retired.views.push_back(m_tab_strip->detach_child(shown.button));
		retired.views.push_back(m_pages->detach_child(shown.page));
	}
	for (ShownSection &shown : m_sections) {
		if (shown.properties != nullptr) {
			retired.properties.push_back(std::move(shown.properties));
		}
	}
	retired.inspectable = std::move(m_inspectable);
	m_tabs.clear();
	m_sections.clear();
	m_shown_tab.clear();
	m_retired.push_back(std::move(retired));
}

void InspectorView::build_header() {
	m_head_icon->set_texture(icon(m_inspectable->icon));
	m_head_icon->set_hidden(m_inspectable->icon.empty());
	m_title->set_text(m_inspectable->title);
	m_title->set_enabled(static_cast<bool>(m_inspectable->rename));
	m_subtitle->set_text(m_inspectable->subtitle);
	m_subtitle->set_tooltip(m_inspectable->subtitle_tooltip);
	m_subtitle->set_hidden(m_inspectable->subtitle.empty());
}

void InspectorView::build_tab(LayoutLoader &loader, InspectorTab &tab) {
	const Usize index = m_tabs.size();

	auto *button = create_as<Button>(loader, "InspectorTabButton", *m_tab_strip);
	if (button == nullptr) {
		return;
	}
	auto *page = loader.create<InspectorPage>(engine_component_file(K_COMPONENTS), "InspectorPage", *m_pages);
	if (page == nullptr) {
		m_tab_strip->remove_child(button);
		return;
	}
	button->set_icon(icon(tab.icon));
	button->set_tooltip(tab.title);
	button->on_click.connect([this, title = tab.title] { select_tab(title); });
	page->root().set_hidden(true);

	m_tabs.push_back({ .tab = &tab, .button = button, .page = &page->root() });
	for (InspectorSection &section : tab.sections) {
		build_section(loader, page->get_sections(), section, index);
	}
	build_actions(loader, page->root(), tab.actions);
}

void InspectorView::build_section(LayoutLoader &loader, View &parent, InspectorSection &section, Usize tab) {
	View *root = nullptr;
	if (section.title.empty()) {
		root = create_as<View>(loader, "InspectorBareSection", parent);
	} else if (auto *collapsible = create_as<Collapsible>(loader, "InspectorPanel", parent,
														  { .attributes = { { "title", section.title } } })) {
		collapsible->set_state_icons(icon("chevron-right"), icon("chevron-down"), true);
		collapsible->set_icon(icon(section.icon));
		for (const InspectorAction &action : section.header_actions) {
			Button *button = collapsible->add_header_action(
				icon(action.icon), action.tooltip.empty() ? action.text : action.tooltip, [run = action.run] {
					if (run) {
						run();
					}
				});
			button->set_enabled(!action.enabled || action.enabled());
		}
		if (const auto it = m_collapsed.find(section.id); it != m_collapsed.end()) {
			collapsible->set_expanded(!it->second);
		}
		collapsible->on_toggled.connect([this, id = section.id](bool expanded) { m_collapsed[id] = !expanded; });
		root = collapsible;
	}
	if (root == nullptr) {
		return;
	}

	auto *grid = root->add_child<PropertyGrid>();
	grid->set_split(true);
	ShownSection shown{ .section = &section, .tab = tab, .root = root, .properties = nullptr, .visible = true };
	if (section.content != nullptr) {
		section.content->notify_edited = [this, &section] {
			if (const auto it = std::ranges::find(m_sections, &section, &ShownSection::section);
				it != m_sections.end()) {
				bind(*it);
			}
			if (section.on_edited) {
				const Delegate<void()> on_edited = section.on_edited;
				on_edited();
			}
		};
		section.content->build_above(*grid);
	}
	if (section.type != nullptr) {
		shown.properties = std::make_unique<ReflectedPropertyGrid>(*grid, *section.type, m_ctx);
	}
	if (section.content != nullptr) {
		section.content->build_below(*grid);
	}
	style_dropdowns(*root);

	m_sections.push_back(std::move(shown));
	bind(m_sections.back());
}

void InspectorView::build_actions(LayoutLoader &loader, View &parent, const std::vector<InspectorAction> &actions) {
	for (const InspectorAction &action : actions) {
		auto *button = create_as<Button>(loader, "InspectorAction", parent);
		if (button == nullptr) {
			continue;
		}
		button->set_icon(icon(action.icon));
		button->set_text(action.text);
		button->set_tooltip(action.tooltip);
		button->set_enabled(!action.enabled || action.enabled());
		button->on_click.connect([run = action.run] {
			if (run) {
				run();
			}
		});
	}
}

void InspectorView::bind(ShownSection &shown) {
	InspectorSection &section = *shown.section;
	void *instance = section.instance ? section.instance() : nullptr;
	shown.visible = section.type == nullptr || instance != nullptr;
	shown.root->set_hidden(!shown.visible);
	if (shown.properties == nullptr || instance == nullptr) {
		return;
	}
	shown.properties->bind(instance, [&section] {
		if (section.content != nullptr) {
			section.content->on_property_edited();
		}
		if (section.on_edited) {
			const Delegate<void()> on_edited = section.on_edited;
			on_edited();
		}
	});
}

bool InspectorView::is_usable(Usize tab) const {
	if (m_tabs[tab].tab->always_visible) {
		return true;
	}
	return std::ranges::any_of(m_sections,
							   [tab](const ShownSection &shown) { return shown.tab == tab && shown.visible; });
}

void InspectorView::refresh_tabs() {
	Option<Usize> preferred;
	Option<Usize> first_usable;
	for (Usize i = 0; i < m_tabs.size(); ++i) {
		const bool usable = is_usable(i);
		m_tabs[i].button->set_hidden(!usable);
		if (!usable) {
			continue;
		}
		if (!first_usable) {
			first_usable = i;
		}
		if (!preferred && m_tabs[i].tab->title == m_preferred_tab) {
			preferred = i;
		}
	}
	if (const Option<Usize> target = preferred ? preferred : first_usable) {
		show_tab(*target);
	} else if (!m_tabs.empty()) {
		show_tab(0);
	}
}

void InspectorView::show_tab(Usize tab) {
	for (Usize i = 0; i < m_tabs.size(); ++i) {
		m_tabs[i].page->set_hidden(i != tab);
		m_tabs[i].button->set_class("props-tab-active", i == tab);
	}
	m_shown_tab = m_tabs[tab].tab->title;
}

void InspectorView::style_dropdowns(View &root) const {
	if (auto *dropdown = dynamic_cast<Dropdown *>(&root)) {
		dropdown->set_variant("props-dropdown");
		dropdown->set_chevron(icon("chevron-down"));
	}
	for (const auto &child : root.get_children()) {
		style_dropdowns(*child);
	}
}

GFX::GfxTexture *InspectorView::icon(std::string_view name) const {
	return m_icons && !name.empty() ? m_icons(name) : nullptr;
}

}
