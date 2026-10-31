#pragma once

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/UI/Core/WindowBehaviour.h"
#include "Core/ProjectManager.h"

namespace Aquila::UI::Core {
class Button;
class Label;
class PopupMenu;
class TabBar;
class TextInput;
class View;
} // namespace Aquila::UI::Core

namespace Editor {

class ProjectLauncher final : public Aquila::UI::Core::WindowBehaviour {
  public:
	ProjectLauncher(std::string layout_path, ProjectManager &project_manager);
	~ProjectLauncher() override;

	void define_behaviour() override;

	Delegate<void(const ProjectInfo &)> on_project_ready;

  private:
	enum class Page : Uint8 { Recent, New, Learn };
	enum class ActionMode : Uint8 { None, Rename, Delete };

	void wire_recent_page();
	void wire_new_page();

	void show_page(Page page);
	void refresh_list();
	void open_project(ProjectInfo project);
	void create_project();
	void select_template(ProjectTemplate project_template);
	void update_new_form();
	void begin_action(ActionMode mode, const ProjectInfo &project);
	void end_action();
	void confirm_action();

	ProjectManager &m_project_manager;

	Page m_page = Page::Recent;
	Aquila::UI::Core::TabBar *m_rail = nullptr;
	Aquila::UI::Core::View *m_pages[3] = {};

	Aquila::UI::Core::Label *m_recent_count = nullptr;
	Aquila::UI::Core::View *m_grid_scroll = nullptr;
	Aquila::UI::Core::View *m_grid_host = nullptr;
	Aquila::UI::Core::View *m_empty_state = nullptr;
	Aquila::UI::Core::PopupMenu *m_context_menu = nullptr;
	Option<ProjectInfo> m_context_project;

	Aquila::UI::Core::View *m_action_bar = nullptr;
	Aquila::UI::Core::Label *m_action_label = nullptr;
	Aquila::UI::Core::TextInput *m_action_input = nullptr;
	Aquila::UI::Core::Button *m_action_confirm = nullptr;
	ActionMode m_action_mode = ActionMode::None;

	Aquila::UI::Core::TextInput *m_name_input = nullptr;
	Aquila::UI::Core::Label *m_location_label = nullptr;
	Aquila::UI::Core::Label *m_error_label = nullptr;
	Aquila::UI::Core::Button *m_create_button = nullptr;
	Aquila::UI::Core::View *m_template_cards[2] = {};
	ProjectTemplate m_template = ProjectTemplate::Empty;
};

} // namespace Editor
