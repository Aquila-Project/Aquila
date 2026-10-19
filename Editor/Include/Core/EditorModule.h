#pragma once

#include "Aquila/Application/EngineContext.h"
#include "Aquila/Application/IModule.h"
#include "Aquila/Rendering/CameraController.h"
#include "Aquila/Rendering/IRenderWindowHost.h"
#include "Aquila/Scene/Entity.h"
#include "Aquila/Scene/SceneStatistics.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/TextureCache.h"
#include "Aquila/UI/Core/UIHost.h"
#include "Aquila/UI/DevTools/UIDevTools.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Core/EditorWindows.h"
#include "Core/ProjectManager.h"
#include "Aquila/UI/Core/DockLayoutSerializer.h"

namespace Aquila::UI::Core {
class Button;
class Canvas;
class DockSpace;
class DockWindowManager;
class View;
} // namespace Aquila::UI::Core

namespace Editor {

class EditorContext;
class EditorWindows;
class Workspaces;
class StatusBar;
class SettingsWindow;
class ProjectLauncher;
class ProjectManager;

/// The editor as an engine module: project launcher, dock shell, menus, shortcuts and preferences.
class EditorModule final : public Aquila::Application::IModule {
  public:
	/// `project` is a project name or folder under ProjectManager::K_ROOT. Set, it opens that project and skips
	/// the launcher (`--project` on the command line).
	explicit EditorModule(std::string project = {});
	~EditorModule() override;

	void on_attach(Aquila::Application::EngineContext &engine) override;
	void on_detach() override;
	void on_pre_render(F32 delta_time) override;
	void on_event(Aquila::Platform::Events::Event &event) override;
	void on_resize(Uint32 width, Uint32 height) override;
	void on_render_resize(Uint32 width, Uint32 height) override;
	void on_play() override;
	void on_pause() override;
	void on_resume() override;
	void on_stop() override;

	/// add_window, add_workspace and add_content_creator also work before on_attach(), they're queued.
	template <typename T> void add_window(EditorWindowType type) {
		type.create = [](EditorContext &context) -> Unique<EditorWindow> { return std::make_unique<T>(context); };
		add_window_type(std::move(type));
	}
	void add_window_type(EditorWindowType type);
	void add_workspace(std::string name, Aquila::UI::Core::DockLayoutDesc preset);
	void add_content_creator(ContentCreator creator);

  private:
	void register_windows();
	void register_workspaces();
	void register_content_creators();
	void register_commands();
	void add_window_command(const EditorWindowType &type);

	[[nodiscard]] Aquila::UI::Core::Canvas *canvas_for(Aquila::Platform::Events::EventSource source) const;
	F32 window_aspect();
	void new_empty_scene();
	void reset_to_demo_scene();
	void scene_replaced();
	void set_outlined_entity(Aquila::SceneManagement::Entity entity);
	/// A script destroyed the selected entity: clear the selection before anything (the inspector) touches its
	/// components again. Runs before every event and frame.
	void drop_destroyed_selection();
	void setup_editor_ui();
	void load_styles();
	void reload_styles();
	void wire_main_menu(Aquila::UI::Core::View *layout_root);
	void open_settings_window();
	void open_project_launcher();
	void enter_editor(const ProjectInfo &project);
	void apply_font_settings();
	void set_ui_scale(F32 scale);
	void save_scene();
	void save_scene_as();
	void open_scene();
	bool load_scene(const std::string &path);
	bool write_scene(const std::string &path);
	[[nodiscard]] bool is_editing() const;
	void start_playing();
	/// Switches to the Game workspace on the next frame, unless a Game view is on screen already.
	void show_game_view();
	void toggle_pause();
	void restore_edit_scene();
	void wire_play_controls(Aquila::UI::Core::View *layout_root);
	/// Feeds the event to the game input while the game has focus, a press inside the Game view gives it focus.
	/// @return true when the game took it.
	bool route_to_game(Aquila::Platform::Events::Event &event);
	/// Losing focus lets go of everything the game had held.
	void set_game_focus(bool focused);
	void sync_play_controls();

	Aquila::Application::EngineContext *m_engine = nullptr;
	Aquila::SceneManagement::StatisticsCollector m_statistics;
	Unique<StatusBar> m_status_bar;
	Aquila::UI::Core::PopupMenu *m_main_menu = nullptr;
	Unique<Aquila::UI::Core::UIHost> m_ui_host;
	Unique<ProjectManager> m_project_manager;
	Unique<Aquila::UI::Core::TextureCache> m_texture_cache;
	Unique<Aquila::Rendering::CameraController> m_editor_camera;

	Unique<EditorContext> m_context;
	Unique<EditorWindows> m_windows;
	Unique<Workspaces> m_workspaces;
	std::vector<EditorWindowType> m_pending_window_types;
	std::vector<std::pair<std::string, Aquila::UI::Core::DockLayoutDesc>> m_pending_workspaces;
	std::vector<ContentCreator> m_pending_content_creators;
	Aquila::UI::Core::DockSpace *m_dock_space = nullptr;
	Unique<Aquila::UI::Core::DockWindowManager> m_dock_manager;

	Unique<Aquila::UI::DevTools::UIDevTools> m_devtools;
	Unique<SettingsWindow> m_settings_window;
	Unique<ProjectLauncher> m_project_launcher;
	Aquila::Rendering::RenderWindowId m_launcher_window = nullptr;
	bool m_editor_entered = false;
	Option<ProjectInfo> m_pending_project;
	std::string m_startup_project;
	ProjectInfo m_project;
	std::string m_scene_path;

	/// The scene as it was when Play was pressed, copied back over the played one on stop.
	Unique<Aquila::SceneManagement::Scene> m_edit_scene;
	/// Set by on_stop(), the restore waits for the next frame so every module has stopped first.
	bool m_restore_pending = false;
	Aquila::UI::Core::Button *m_play_button = nullptr;
	Aquila::UI::Core::Button *m_pause_button = nullptr;
	Aquila::UI::Core::Button *m_stop_button = nullptr;
	/// Views that take the play-mode tint: top bar, status bar and the gaps between docked panels.
	std::vector<Aquila::UI::Core::View *> m_play_tinted;
	/// Where Play came from, Stop goes back there if the Game workspace is still the active one.
	std::string m_workspace_before_play;
	/// Applied at the start of the next frame, so no panel is destroyed while one of its buttons is being clicked.
	Option<std::string> m_pending_workspace;

	Aquila::SceneManagement::Entity m_outlined_entity;
	Option<Aquila::SceneManagement::Entity> m_pending_pick;

	Aquila::UI::Core::LayoutLoader m_layout_loader;
};

} // namespace Editor
