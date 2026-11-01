#ifndef AQUILA_EDITOR_EDITOR_SHELL_H
#define AQUILA_EDITOR_EDITOR_SHELL_H

#include "Aquila/Application/EngineContext.h"
#include "Aquila/UI/Core/Behaviour.h"
#include "UI/Panels/StatusBar.h"

namespace Aquila::UI::Core {
class Button;
class DockSpace;
class PopupMenu;
class TabBar;
}

namespace Editor {

class EditorContext;
class EditorWindows;

class EditorShell final : public Aquila::UI::Core::Behaviour {
  public:
	EditorShell(EditorContext &context, const EditorWindows &windows);

	void define_behaviour() override;

	[[nodiscard]] Aquila::UI::Core::DockSpace *get_dock_space() const { return m_dock_space; }
	[[nodiscard]] Aquila::UI::Core::TabBar *get_workspace_tabs() const { return m_workspace_tabs; }

	void set_play_state(Aquila::Application::PlayState state);

	void set_status_visible(bool visible) { m_status.set_visible(visible); }
	[[nodiscard]] bool is_status_visible() const { return m_status.is_visible(); }
	void update_status(const Aquila::SceneManagement::SceneStatistics &statistics, const std::string &selected_name);

  private:
	void add_window_entries();

	EditorContext &m_context;
	const EditorWindows &m_windows;
	Aquila::UI::Core::DockSpace *m_dock_space = nullptr;
	Aquila::UI::Core::TabBar *m_workspace_tabs = nullptr;
	Aquila::UI::Core::Button *m_play = nullptr;
	Aquila::UI::Core::Button *m_pause = nullptr;
	Aquila::UI::Core::Button *m_stop = nullptr;
	Aquila::UI::Core::PopupMenu *m_menu = nullptr;
	std::vector<Aquila::UI::Core::View *> m_tinted;
	StatusBar m_status;
};

}

#endif
