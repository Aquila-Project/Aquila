#include "Core/Workspaces.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Core/EditorWindows.h"

namespace Editor {

void Workspaces::add(std::string name, Aquila::UI::Core::DockLayoutDesc preset) {
	if (find(name) != nullptr) {
		AQUILA_LOG_WARNING("Workspaces: '{}' is already registered", name);
		return;
	}
	m_entries.push_back({ .name = std::move(name), .preset = std::move(preset) });
	if (m_tab_container != nullptr) {
		create_tab(m_entries.back());
		refresh_tabs();
	}
}

Workspaces::Entry *Workspaces::find(std::string_view name) {
	auto it = std::ranges::find_if(m_entries, [name](const Entry &entry) { return entry.name == name; });
	return it != m_entries.end() ? &*it : nullptr;
}

void Workspaces::build_tabs(Aquila::UI::Core::View &container) {
	m_tab_container = &container;
	for (Entry &entry : m_entries) {
		create_tab(entry);
	}
	refresh_tabs();
}

void Workspaces::create_tab(Entry &entry) {
	auto *tab = m_tab_container->add_child<Aquila::UI::Core::Button>(entry.name);
	tab->add_class("workspace-tab");
	tab->on_click.connect([this, name = entry.name] { activate(name); });
	entry.tab = tab;
}

void Workspaces::activate(std::string_view name) {
	Entry *target = find(name);
	if (target == nullptr || m_active == name) {
		return;
	}
	if (Entry *current = find(m_active)) {
		current->current = m_windows.capture();
	}
	m_active = target->name;
	m_windows.apply_layout(target->current.value_or(target->preset));
	refresh_tabs();
	on_activated(m_active);
}

void Workspaces::activate_relative(int offset) {
	if (m_entries.empty()) {
		return;
	}
	const auto count = static_cast<int>(m_entries.size());
	const auto it = std::ranges::find(m_entries, m_active, &Entry::name);
	const int current = it == m_entries.end() ? 0 : static_cast<int>(it - m_entries.begin());
	const std::string target = m_entries[static_cast<Usize>((((current + offset) % count) + count) % count)].name;
	activate(target);
}

void Workspaces::reset_active() {
	Entry *current = find(m_active);
	if (current == nullptr) {
		return;
	}
	current->current.reset();
	m_windows.apply_layout(current->preset);
}

void Workspaces::refresh_tabs() {
	for (Entry &entry : m_entries) {
		if (entry.tab != nullptr) {
			entry.tab->set_class("workspace-tab-active", entry.name == m_active);
		}
	}
}

} // namespace Editor
