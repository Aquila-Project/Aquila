#pragma once

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/UI/Core/WindowBehaviour.h"
#include "Core/ProjectManager.h"

namespace Aquila::UI::Core {
class Button;
class Label;
class PopupMenu;
class TextInput;
class View;
} // namespace Aquila::UI::Core

namespace Editor {

class ProjectLauncher final : public Aquila::UI::Core::WindowBehaviour {
  public:
	ProjectLauncher(std::string layout_path, ProjectManager &project_manager);
	~ProjectLauncher() override;

	void define_behaviour() override;
	/// Keeps the page and what was typed in the new project form.
	void take_state(Aquila::UI::Core::WindowBehaviour &previous) override;

	Delegate<void(const ProjectInfo &)> on_project_ready;

  private:
	enum class Page : Uint8 { Home, New };
	enum class ActionMode : Uint8 { None, Rename, Delete, Forget };

	void wire_home_page();
	void wire_new_page();
	void wire_menu(Aquila::UI::Core::PopupMenu *menu);

	void show_page(Page page);
	void refresh_list();
	void browse_for_project();
	void browse_location();
	void open_project(ProjectInfo project);
	void create_project();
	/// "New Project", or "New Project 2" and up when that one is taken in the chosen location.
	[[nodiscard]] std::string default_project_name() const;
	/// @return true when the form can create a project
	bool update_new_form();
	void begin_action(ActionMode mode, const ProjectInfo &project);
	void end_action();
	void confirm_action();

	ProjectManager &m_project_manager;
	Page m_page = Page::Home;
	Option<std::string> m_restored_name;
	Option<std::string> m_restored_location;

	Aquila::UI::Core::View *m_home_page = nullptr;
	Aquila::UI::Core::View *m_new_page = nullptr;

	Aquila::UI::Core::Label *m_open_error = nullptr;
	Aquila::UI::Core::View *m_recent_scroll = nullptr;
	Aquila::UI::Core::View *m_recent_host = nullptr;
	Aquila::UI::Core::View *m_empty_state = nullptr;
	Aquila::UI::Core::PopupMenu *m_context_menu = nullptr;
	Aquila::UI::Core::PopupMenu *m_external_menu = nullptr;
	Option<ProjectInfo> m_context_project;

	Aquila::UI::Core::View *m_action_bar = nullptr;
	Aquila::UI::Core::Label *m_action_label = nullptr;
	Aquila::UI::Core::TextInput *m_action_input = nullptr;
	Aquila::UI::Core::Button *m_action_confirm = nullptr;
	ActionMode m_action_mode = ActionMode::None;

	Aquila::UI::Core::TextInput *m_name_input = nullptr;
	Aquila::UI::Core::Label *m_name_error = nullptr;
	Aquila::UI::Core::TextInput *m_location_input = nullptr;
	Aquila::UI::Core::Label *m_location_error = nullptr;
	Aquila::UI::Core::Label *m_preview = nullptr;
	Aquila::UI::Core::Label *m_error_label = nullptr;
	Aquila::UI::Core::Button *m_create_button = nullptr;
};

} // namespace Editor
