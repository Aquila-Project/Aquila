#include "Core/Workspaces.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/UI/Widgets/TabBar.h"
#include "Core/EditorWindows.h"

namespace Editor {

void Workspaces::add(std::string name, Aquila::UI::Core::DockLayoutDesc preset) {
	if (find(name) != nullptr) {
		AQUILA_LOG_WARNING("Workspaces: '{}' is already registered", name);
		return;
	}
	m_entries.push_back({ .name = std::move(name), .preset = std::move(preset) });
	if (m_tabs != nullptr) {
		m_tabs->add_tab(m_entries.back().name);
		refresh_tabs();
	}
}

Workspaces::Entry *Workspaces::find(std::string_view name) {
	auto it = std::ranges::find_if(m_entries, [name](const Entry &entry) { return entry.name == name; });
	return it != m_entries.end() ? &*it : nullptr;
}

void Workspaces::build_tabs(Aquila::UI::Core::TabBar &tabs) {
	m_tabs = &tabs;
	m_tabs->on_selected.connect([this](int index) { activate(m_entries[static_cast<Usize>(index)].name); });
	for (const Entry &entry : m_entries) {
		m_tabs->add_tab(entry.name);
	}
	refresh_tabs();
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
	if (m_tabs == nullptr) {
		return;
	}
	const auto it = std::ranges::find(m_entries, m_active, &Entry::name);
	m_tabs->set_active(it == m_entries.end() ? -1 : static_cast<int>(it - m_entries.begin()));
}

} // namespace Editor
