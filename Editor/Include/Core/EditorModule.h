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
class Canvas;
class DockSpace;
class DockWindowManager;
class View;
}

namespace Editor {

class EditorContext;
class EditorWindows;
class Workspaces;
class StatusBar;
class SettingsWindow;
class ProjectLauncher;
class ProjectManager;

class EditorModule final : public Aquila::Application::IModule {
  public:
	EditorModule();
	~EditorModule() override;

	void on_attach(Aquila::Application::EngineContext &engine) override;
	void on_detach() override;
	void on_pre_render(F32 delta_time) override;
	void on_event(Aquila::Platform::Events::Event &event) override;
	void on_resize(Uint32 width, Uint32 height) override;
	void on_render_resize(Uint32 width, Uint32 height) override;

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

	[[nodiscard]] Aquila::UI::Core::Canvas *canvas_for(Aquila::Platform::Events::EventSource source) const;
	F32 window_aspect();
	void new_empty_scene();
	void reset_to_demo_scene();
	void scene_replaced();
	void set_outlined_entity(Aquila::SceneManagement::Entity entity);
	void setup_editor_ui();
	void load_styles();
	void reload_styles();
	void wire_main_menu(Aquila::UI::Core::View *layout_root);
	void open_settings_window();
	void open_project_launcher();
	void enter_editor(const ProjectInfo &project);
	void apply_font_settings();
	void set_ui_scale(F32 scale);

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

	Aquila::SceneManagement::Entity m_outlined_entity;
	Option<Aquila::SceneManagement::Entity> m_pending_pick;

	Aquila::UI::Core::LayoutLoader m_layout_loader;
};

}
