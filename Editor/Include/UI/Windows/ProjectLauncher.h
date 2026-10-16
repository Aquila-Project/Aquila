#pragma once

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Core/ProjectManager.h"

namespace Aquila::Graphics {
class Renderer2D;
}
namespace Aquila::GFX {
class GfxCommandList;
class GfxTexture;
} // namespace Aquila::GFX
namespace Aquila::Platform::Events {
class Event;
}
namespace Aquila::UI::Core {
class Button;
class Canvas;
class Label;
class PopupMenu;
class TextInput;
class TextureCache;
class View;
} // namespace Aquila::UI::Core

namespace Editor {

class ProjectLauncher {
  public:
	ProjectLauncher();
	~ProjectLauncher();

	void build(ProjectManager *project_manager, Aquila::UI::Core::TextureCache *texture_cache, Uint32 width,
			   Uint32 height, const std::vector<std::string> &style_paths);

	void update(F32 delta_time);
	void render(Aquila::Graphics::Renderer2D &batcher, Aquila::GFX::GfxCommandList &cmd);
	void on_event(Aquila::Platform::Events::Event &event);
	void set_frame_target(const void *target);

	Delegate<void(const ProjectInfo &)> on_project_ready;

  private:
	enum class Page : Uint8 { Recent, New, Learn };
	enum class ActionMode : Uint8 { None, Rename, Delete };

	void build_header(Aquila::UI::Core::View *root);
	void build_rail(Aquila::UI::Core::View *main);
	void build_recent_page(Aquila::UI::Core::View *content);
	void build_new_page(Aquila::UI::Core::View *content);
	void build_learn_page(Aquila::UI::Core::View *content);

	void show_page(Page page);
	void refresh_list();
	void open_project(ProjectInfo project);
	void create_project();
	void select_template(ProjectTemplate project_template);
	void update_new_form();
	void begin_action(ActionMode mode, const ProjectInfo &project);
	void end_action();
	void confirm_action();

	Unique<Aquila::UI::Core::Canvas> m_canvas;
	ProjectManager *m_project_manager = nullptr;
	Aquila::UI::Core::TextureCache *m_texture_cache = nullptr;
	Aquila::GFX::GfxTexture *m_logo = nullptr;

	Page m_page = Page::Recent;
	Aquila::UI::Core::Button *m_nav_buttons[3] = {};
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
