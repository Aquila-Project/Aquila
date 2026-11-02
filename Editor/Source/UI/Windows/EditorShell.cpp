#include "UI/Windows/EditorShell.h"

#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/DockSpace.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Aquila/UI/Widgets/TabBar.h"
#include "Core/EditorContext.h"
#include "Core/EditorWindows.h"

namespace Editor {

using namespace Aquila::UI::Core;
using Aquila::Application::PlayState;

EditorShell::EditorShell(EditorContext &context, const EditorWindows &windows)
	: m_context(context), m_windows(windows) {}

void EditorShell::define_behaviour() {
	m_dock_space = get<DockSpace>("editor-dock");
	m_workspace_tabs = get<TabBar>("workspace-tabs");
	m_play = get<Button>("topbar-play");
	m_pause = get<Button>("topbar-pause");
	m_stop = get<Button>("topbar-stop");
	for (const std::string_view id : { "topbar", "dock-root", "hud-status" }) {
		if (View *view = get(id)) {
			m_tinted.push_back(view);
		}
	}
	if (m_dock_space != nullptr) {
		m_tinted.push_back(m_dock_space);
	}

	m_menu = get<PopupMenu>("main-menu");
	if (m_menu != nullptr) {
		m_menu->set_submenu_icon(m_context.icon("chevron-right"));
		add_window_entries();
		if (auto *menu_button = get<Button>("topbar-menu")) {
			menu_button->on_click.connect([this, menu_button] {
				const Rect rect = menu_button->get_absolute_rect();
				m_menu->open_at({ rect.position.x, rect.position.y + rect.size.y + 4.F });
			});
		}
	}
	m_status.build(&root());
}

void EditorShell::add_window_entries() {
	auto *window_menu = get<PopupMenu>("window-menu");
	if (window_menu == nullptr) {
		return;
	}
	Usize index = 0;
	for (const EditorWindowType &type : m_windows.get_types()) {
		const std::string id = "window.open." + type.id;
		const Command *command = m_context.commands.find(id);
		if (command == nullptr) {
			AQUILA_LOG_WARNING("EditorShell: menu command '{}' is not registered", id);
			continue;
		}
		window_menu->insert_item(index++, command->title, m_context.shortcuts.get_label(id), m_context.icon(type.icon),
								 [this, id] { m_context.commands.execute(id); });
	}
}

void EditorShell::set_play_state(PlayState state) {
	if (m_play != nullptr) {
		m_play->set_class("topbar-btn-active", state != PlayState::Stopped);
	}
	if (m_pause != nullptr) {
		m_pause->set_enabled(state != PlayState::Stopped);
		m_pause->set_class("topbar-btn-active", state == PlayState::Paused);
	}
	if (m_stop != nullptr) {
		m_stop->set_enabled(state != PlayState::Stopped);
	}
	for (View *view : m_tinted) {
		view->set_class("play-tint", state != PlayState::Stopped);
	}
}

void EditorShell::update_status(const Aquila::SceneManagement::SceneStatistics &statistics,
								const std::string &selected_name) {
	m_status.update(statistics, selected_name);
}

}
