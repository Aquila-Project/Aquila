#include "Core/EditorModule.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/ServiceRegistry.h"
#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/Graphics/Material/MaterialAssets.h"
#include "Aquila/Graphics/Shader/ShaderHotReload.h"
#include "Aquila/Graphics/Texture/TextureLibrary.h"
#include "Aquila/Scripting/LuauConfig.h"
#include "Aquila/Scripting/ScriptApi.h"
#include "Core/AssetThumbnails.h"
#include "Core/DefaultShortcuts.h"
#include "Core/EditorContext.h"
#include "Core/EditorWindows.h"
#include "Core/FileDialog.h"
#include "Core/ProjectManager.h"
#include "Core/Workspaces.h"

#include "Aquila/Application/ApplicationNew.h"

#include "Aquila/UI/Core/FontManager.h"
#include "Aquila/UI/DevTools/UIDevTools.h"
#include "UI/EditorWindows/ConsoleWindow.h"
#include "UI/EditorWindows/ContentBrowserWindow.h"
#include "UI/EditorWindows/GameWindow.h"
#include "UI/EditorWindows/HierarchyWindow.h"
#include "UI/EditorWindows/InspectorWindow.h"
#include "UI/EditorWindows/TodoWindow.h"
#include "UI/EditorWindows/ViewportWindow.h"
#include "UI/Windows/BuildWindow.h"
#include "UI/Windows/ProjectLauncher.h"
#include "UI/Windows/EditorShell.h"
#include "UI/Windows/SettingsWindow.h"
#include "UI/Viewport/TransformGizmo.h"
#include "UI/Viewport/ViewportOverlay.h"
#include "UI/Widgets/ChordButton.h"
#include "UI/Widgets/RenameField.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Platform/Events/InputEvent.h"
#include "Aquila/Platform/Events/KeyChord.h"
#include "Aquila/Platform/Input.h"
#include "Aquila/Platform/InputState.h"
#include "Aquila/Rendering/RenderPipeline.h"
#include "Aquila/Rendering/Systems/ObjectPickingSystem.h"
#include "Aquila/Scene/Components/OutlineComponent.h"
#include "Aquila/Scene/DefaultScenes.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/UI/Core/DockLayoutBuilder.h"
#include "Aquila/UI/Core/DockWindowManager.h"
#include "Aquila/UI/Core/FontRegistry.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/UIHost.h"
#include "Aquila/UI/Widgets/AssetSlot.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/ColorPicker.h"
#include "Aquila/UI/Widgets/DockSpace.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Aquila/UI/Widgets/TabBar.h"
#include "Aquila/UI/Widgets/TextInput.h"

#include "Aquila/Scene/Scene.h"
#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Foundation/FrameScheduler.h"

namespace Editor {

using namespace Aquila;
using namespace Aquila::SceneManagement;
using namespace Aquila::SceneManagement::Components;
using namespace Aquila::Application;
using namespace Aquila::UI::Core;
namespace Events = Aquila::Platform::Events;

namespace {
constexpr std::string_view K_GAME_WORKSPACE = "Game";
constexpr std::string_view K_RELOAD_LAYOUTS = "editor.reload_layouts";
constexpr F64 K_SETTINGS_SAVE_SECONDS = 0.5;
constexpr auto K_SETTINGS_SAVE_DELAY = std::chrono::milliseconds(500);

std::string layout_dir() {
	return Config::get_preferences().ui.layout_dir;
}
}

EditorModule::EditorModule(std::string project) : m_startup_project(std::move(project)) {}

EditorModule::~EditorModule() = default;

void EditorModule::on_attach(EngineContext &engine) {
	m_engine = &engine;

	Config::get_preferences().load_from_file();
	m_applied_preferences = Config::get_preferences();

	Window &main_window = m_engine->get_window();
	m_ui_host = std::make_unique<UIHost>(UIHostDesc{
		.overlay_renderer = m_engine->get_overlay_renderer(),
		.width = main_window.get_width(),
		.height = main_window.get_height(),
		.clipboard_get = [&main_window] { return main_window.get_clipboard_text(); },
		.clipboard_set = [&main_window](const std::string &text) { main_window.set_clipboard_text(text); },
		.base_styles = Config::get_preferences().ui.style_paths,
	});
	m_layout_service = m_engine->get_services().provide<LayoutLoader>(m_ui_host->get_layout_loader());
	FontRegistry::set_ui_scale(Config::get_preferences().ui_scale);
	if (Aquila::Graphics::Shader::ShaderHotReload::is_alive()) {
		Aquila::Graphics::Shader::ShaderHotReload::get()->enable(Config::get_preferences().advanced.shader_hot_reload);
	}

	m_user_fonts = FontManager::get().add_font_dir(Config::get_preferences().ui.user_fonts_dir);
	FontManager::get().initialize(m_engine->get_context(), Config::get_preferences().fonts);

	m_project_manager = std::make_unique<ProjectManager>();
	m_texture_cache =
		std::make_unique<TextureCache>(m_engine->get_context(), Config::get_preferences().ui.resources_path);

	LayoutLoader &layouts = m_ui_host->get_layout_loader();
	layouts.register_font("regular", FontManager::get().get_font("regular"));
	layouts.register_texture_cache(m_texture_cache.get());
	layouts.register_widget("ColorPicker", [this](std::string_view, Aquila::UI::Text::FontAtlas *) -> Unique<View> {
		return std::make_unique<ColorPicker>(m_engine->get_context());
	});
	layouts.register_widget("RenameField", [](std::string_view, Aquila::UI::Text::FontAtlas *) -> Unique<View> {
		return std::make_unique<RenameField>();
	});
	layouts.register_widget("ChordButton", [](std::string_view text, Aquila::UI::Text::FontAtlas *) -> Unique<View> {
		return std::make_unique<ChordButton>(std::string(text));
	});
	layouts.register_widget("TransformGizmo", [this](std::string_view, Aquila::UI::Text::FontAtlas *) -> Unique<View> {
		return m_context ? std::make_unique<TransformGizmo>(*m_context) : nullptr;
	});
	layouts.register_widget("ViewportOverlay", [this](std::string_view, Aquila::UI::Text::FontAtlas *) -> Unique<View> {
		return m_context ? std::make_unique<ViewportOverlay>(*m_context) : nullptr;
	});

	if (!m_startup_project.empty()) {
		const std::string directory = std::string(ProjectManager::K_ROOT) + "/" +
			Aquila::Foundation::to_file_name(std::filesystem::path(m_startup_project).filename().string());
		if (Option<ProjectInfo> project = m_project_manager->load(directory)) {
			m_pending_project = *project;
			Aquila::Foundation::FrameScheduler::get()->request_frame();
			return;
		}
		AQUILA_LOG_ERROR("EditorModule: no project at {}", directory);
	} else if (Config::get_preferences().startup.reopen_last_project) {
		const std::vector<ProjectInfo> projects = m_project_manager->list();
		if (!projects.empty()) {
			m_pending_project = projects.front();
			Aquila::Foundation::FrameScheduler::get()->request_frame();
			return;
		}
	}
	open_project_launcher();
}

void EditorModule::open_project_launcher() {
	m_project_launcher = std::make_unique<BehaviourWindow>(
		m_ui_host->get_layout_loader(),
		[this]() -> Unique<WindowBehaviour> {
			auto launcher =
				std::make_unique<ProjectLauncher>(layout_dir() + "Windows/launcher.aqlayout", *m_project_manager);
			launcher->on_project_ready = [this](const ProjectInfo &project) {
				m_pending_project = project;
				Aquila::Foundation::FrameScheduler::get()->request_frame();
			};
			return launcher;
		},
		900, 600);
	m_project_launcher->set_event_hook(
		[this](Aquila::Platform::Events::Event &event) { return handle_window_key(event); });

	m_project_launcher->open(m_engine->get_window_host(), "Aquila - Projects", [this] {
		m_project_launcher.reset();
		if (!m_editor_entered) {
			m_engine->request_close();
		}
	});
}

void EditorModule::enter_editor(const ProjectInfo &project) {
	AQUILA_LOG_INFO("EditorModule: opening project '{}' ({})", project.name, project.directory);

	Aquila::Graphics::MaterialAssets::get()->clear();
	Aquila::Graphics::TextureLibrary::get()->clear();
	if (m_thumbnails) {
		m_thumbnails->clear();
	}
	if (m_texture_cache) {
		m_texture_cache->evict_project_assets();
	}

	m_editor_entered = true;
	m_project = project;
	m_project_manager->open(m_project);
	if (m_project_launcher) {
		m_project_launcher->close();
	}

	m_engine->get_window().show();
	m_editor_camera = std::make_unique<Aquila::Rendering::CameraController>();
	setup_editor_ui();

	m_context->set_project_settings_writer(
		[this](ProjectSettings settings) { write_project_settings(std::move(settings)); });
	apply_preferences(true);

	const bool loaded = !m_project.scenes.empty() &&
		load_scene(std::string{ SharedConstants::PROJECT_SCENES_DIR } + "/" + m_project.scenes.front());
	if (!loaded) {
		new_empty_scene();
	}

	const Usize preloading =
		Aquila::Graphics::MaterialAssets::get()->preload_textures(std::string{ SharedConstants::PROJECT_CONTENT_DIR });
	AQUILA_LOG_INFO("EditorModule: loading {} textures in the background ({} for the scene)",
					Aquila::Graphics::TextureLibrary::get()->get_loading_count(),
					Aquila::Graphics::TextureLibrary::get()->get_loading_count() - preloading);
}

void EditorModule::on_detach() {
	save_settings_if_due(true);
	AssetSlot::set_adapter("entity", {});
	if (m_context) {
		for (const AssetType &type : m_context->asset_types.get_all()) {
			if (type.thumbnail != AssetThumbnail::None) {
				AssetSlot::set_adapter(type.id, {});
			}
		}
	}
	if (m_windows) {
		m_windows->shutdown();
	}
	m_dock_manager.reset();
	m_edit_scene.reset();
	m_settings_window.reset();
	m_build_window.reset();
	m_project_launcher.reset();
	m_devtools.reset();

	m_layout_service.reset();
	m_ui_host.reset();
	m_workspaces.reset();
	m_windows.reset();
	m_context.reset();
	m_thumbnails.reset();
	m_texture_cache.reset();
	FontManager::get().shutdown();
}

void EditorModule::on_pre_render(F32 delta_time) {
	if (m_pending_project) {
		const ProjectInfo project = *m_pending_project;
		m_pending_project.reset();
		enter_editor(project);
	}
	if (m_layout_reload_pending) {
		m_layout_reload_pending = false;
		reload_layouts();
	}
	save_settings_if_due(false);
	update_luau_config();

	if (m_restore_pending) {
		restore_edit_scene();
	}
	if (m_context && !m_context->get_game_view()) {
		set_game_focus(false);
	}
	if (m_pending_workspace && m_workspaces) {
		const std::string workspace = std::move(*m_pending_workspace);
		m_pending_workspace.reset();
		m_workspaces->activate(workspace);
	}

	drop_destroyed_selection();

	if (m_pending_pick && m_context) {
		const Entity picked = *m_pending_pick;
		m_pending_pick.reset();
		if (picked.is_valid()) {
			m_context->selection.select(picked);
		} else {
			m_context->selection.clear();
		}
	}

	if (m_shell != nullptr && m_shell->is_status_visible()) {
		const bool has_selection = m_outlined_entity.is_valid() && m_outlined_entity.exists();
		m_shell->update_status(m_statistics.collect(m_engine->get_scene(), m_outlined_entity),
							   has_selection ? m_outlined_entity.get_name() : std::string());
	}

	if (m_thumbnails) {
		m_thumbnails->update();
	}
	if (m_windows) {
		m_windows->update(delta_time);
	}
	m_ui_host->update(delta_time);

	m_engine->get_window().set_cursor(m_ui_host->get_active_cursor());
}

Canvas *EditorModule::canvas_for(Events::EventSource source) const {
	if (source == m_engine->get_window_host().get_main_window()) {
		return &m_ui_host->get_canvas(UILayer::Editor);
	}
	return m_dock_manager ? m_dock_manager->find_canvas(source) : nullptr;
}

void EditorModule::drop_destroyed_selection() {
	if (m_context && !m_context->selection.get().is_null() && !m_context->selection.has()) {
		m_context->selection.clear();
	}
}

void EditorModule::on_event(Events::Event &event) {
	drop_destroyed_selection();
	Canvas *source_canvas = canvas_for(event.get_source());
	if (source_canvas == nullptr) {
		return;
	}
	const bool from_main = event.get_source() == m_engine->get_window_host().get_main_window();
	const bool to_game = from_main && route_to_game(event);
	const bool game_keys = to_game && (event.get_category() & Events::EventCategory::Keyboard);

	if (m_editor_camera && !to_game) {
		m_editor_camera->on_event(event);
	}

	if (from_main && m_devtools && m_devtools->on_event(event)) {
		return;
	}

	if (from_main && !game_keys) {
		m_ui_host->on_event(event);
	}

	Events::EventDispatcher dispatcher(event);
	dispatcher.dispatch<Events::KeyPressedEvent>([this, source_canvas, to_game](Events::KeyPressedEvent &e) {
		if (!m_context) {
			return false;
		}
		const bool typing = to_game || view_is<TextInput>(source_canvas->get_focused_view());
		return m_context->shortcuts.handle(Events::KeyChord::from(e), e.is_repeat(), typing);
	});
}

bool EditorModule::route_to_game(Events::Event &event) {
	const Option<Rect> view = m_context ? m_context->get_game_view() : std::nullopt;
	if (!view) {
		set_game_focus(false);
		return false;
	}
	Events::EventDispatcher dispatcher(event);
	dispatcher.dispatch<Events::MouseButtonPressedEvent>([this, &view](Events::MouseButtonPressedEvent &) {
		set_game_focus(view->contains(Platform::Input::get_mouse_position()));
		return false;
	});
	if (!m_context->is_game_focused()) {
		return false;
	}
	m_engine->get_game_input().on_event(event);
	return true;
}

void EditorModule::set_game_focus(bool focused) {
	if (!m_context || m_context->is_game_focused() == focused) {
		return;
	}
	m_context->set_game_focused(focused);
	if (!focused) {
		m_engine->get_game_input().release_all();
	}
}

void EditorModule::set_outlined_entity(Entity entity) {
	if (m_outlined_entity.is_valid() && m_outlined_entity.exists()) {
		m_outlined_entity.try_remove_component<OutlineComponent>();
	}
	m_outlined_entity = entity;
	if (m_outlined_entity.is_valid()) {
		m_outlined_entity.add_component<OutlineComponent>();
	}
}

void EditorModule::on_render_resize(Uint32, Uint32) {
	if (m_context) {
		m_context->on_render_output_changed();
	}
}

void EditorModule::on_play() {
	sync_play_controls();
}

void EditorModule::on_pause() {
	sync_play_controls();
}

void EditorModule::on_resume() {
	sync_play_controls();
}

void EditorModule::on_stop() {
	m_restore_pending = m_edit_scene != nullptr;
	const std::string back = std::exchange(m_workspace_before_play, {});
	if (m_pending_workspace) {
		m_pending_workspace.reset();
	} else if (!back.empty() && Config::get_preferences().play.return_after_stop && m_workspaces &&
			   m_workspaces->get_active() == K_GAME_WORKSPACE) {
		m_pending_workspace = back;
	}
	sync_play_controls();
}

bool EditorModule::is_editing() const {
	return m_engine->get_play_state() == PlayState::Stopped;
}

void EditorModule::start_playing() {
	if (m_engine->get_play_state() == PlayState::Paused) {
		m_engine->resume();
		return;
	}
	restore_edit_scene();
	Scene &scene = m_engine->get_scene();
	m_edit_scene = std::make_unique<Scene>();
	m_edit_scene->copy_from(scene);
	if (const Entity camera = scene.find_game_camera(); camera.is_valid()) {
		scene.set_active_camera(camera);
	}
	m_engine->play();
	show_game_view();
	AQUILA_LOG_INFO("EditorModule: playing '{}'", scene.get_scene_name());
}

void EditorModule::show_game_view() {
	if (!Config::get_preferences().play.switch_to_game_view || !m_workspaces ||
		m_context->is_window_visible(GameWindow::K_TYPE_ID) || m_workspaces->get_active() == K_GAME_WORKSPACE) {
		return;
	}
	m_workspace_before_play = m_workspaces->get_active();
	m_pending_workspace = std::string(K_GAME_WORKSPACE);
}

void EditorModule::toggle_pause() {
	if (m_engine->get_play_state() == PlayState::Paused) {
		m_engine->resume();
	} else {
		m_engine->pause();
	}
}

void EditorModule::restore_edit_scene() {
	m_restore_pending = false;
	if (!m_edit_scene) {
		return;
	}
	const Entity selected = m_context->selection.get();
	m_engine->get_scene().copy_from(*m_edit_scene);
	m_edit_scene.reset();
	scene_replaced();
	if (selected.exists()) {
		m_context->selection.select(selected);
	}
	AQUILA_LOG_INFO("EditorModule: stopped, scene restored");
}

void EditorModule::sync_play_controls() {
	if (m_shell != nullptr) {
		m_shell->set_play_state(m_engine->get_play_state());
	}
}

F32 EditorModule::window_aspect() {
	return static_cast<F32>(m_engine->get_window().get_width()) / static_cast<F32>(m_engine->get_window().get_height());
}

void EditorModule::scene_replaced() {
	if (m_context) {
		m_context->selection.clear();
		m_context->selection.on_scene_replaced();
	}
}

void EditorModule::new_empty_scene() {
	m_engine->get_scene().clear();
	m_engine->get_scene().set_scene_name("Untitled");
	m_scene_path.clear();
	SceneManagement::spawn_default_camera(m_engine->get_scene(), window_aspect());
	scene_replaced();
	AQUILA_LOG_INFO("EditorModule: new empty scene");
}

void EditorModule::save_scene() {
	if (m_scene_path.empty()) {
		save_scene_as();
		return;
	}
	write_scene(m_scene_path);
}

void EditorModule::save_scene_as() {
	const std::array filters = { FileDialog::Filter{ .name = "Aquila scene", .extensions = "aqscene" } };
	const std::string folder = m_context->to_native(std::string(SharedConstants::PROJECT_SCENES_DIR));
	Option<std::string> native =
		FileDialog::save_file(filters, folder, m_engine->get_scene().get_scene_name() + ".aqscene");
	if (!native) {
		return;
	}
	std::filesystem::path file(*native);
	if (file.extension() != ".aqscene") {
		file += ".aqscene";
	}
	const Option<std::string> path = m_context->to_project_path(file.string());
	if (!path) {
		AQUILA_LOG_ERROR("EditorModule: scenes have to be saved inside the project folder ({})",
						 m_project.native_directory);
		return;
	}
	m_engine->get_scene().set_scene_name(file.stem().string());
	write_scene(*path);
}

bool EditorModule::write_scene(const std::string &path) {
	std::error_code error;
	std::filesystem::create_directories(std::filesystem::path(m_context->to_native(path)).parent_path(), error);
	if (!m_engine->get_scene().serialize(path)) {
		AQUILA_LOG_ERROR("EditorModule: couldn't save the scene to {}", path);
		return false;
	}
	m_scene_path = path;
	const std::string mount = SharedConstants::PROJECT_MOUNT + "/";
	if (path.starts_with(mount)) {
		m_project_manager->add_scene(m_project, path.substr(mount.size()));
	}
	AQUILA_LOG_INFO("EditorModule: scene saved at {}", path);
	return true;
}

void EditorModule::open_scene() {
	const std::array filters = { FileDialog::Filter{ .name = "Aquila scene", .extensions = "aqscene" } };
	const Option<std::string> native =
		FileDialog::open_file(filters, m_context->to_native(std::string(SharedConstants::PROJECT_SCENES_DIR)));
	if (!native) {
		return;
	}
	const Option<std::string> path = m_context->to_project_path(*native);
	if (!path) {
		AQUILA_LOG_ERROR("EditorModule: {} isn't inside the project folder", *native);
		return;
	}
	load_scene(*path);
}

bool EditorModule::load_scene(const std::string &path) {
	if (!m_engine->get_scene().deserialize(path)) {
		AQUILA_LOG_ERROR("EditorModule: couldn't open the scene {}", path);
		return false;
	}
	m_scene_path = path;
	scene_replaced();
	AQUILA_LOG_INFO("EditorModule: scene opened from {}", path);
	return true;
}

void EditorModule::reset_to_demo_scene() {
	m_engine->get_scene().clear();
	SceneManagement::populate_demo_scene(m_engine->get_scene(), m_engine->get_context(), window_aspect());
	scene_replaced();
	AQUILA_LOG_INFO("EditorModule: reset to demo scene");
}

void EditorModule::register_windows() {
	m_windows->add<ViewportWindow>({ .id = "viewport", .title = "Viewport", .icon = "video", .group = "Scene" });
	m_windows->add<HierarchyWindow>({ .id = "hierarchy", .title = "Hierarchy", .icon = "list-tree", .group = "Scene" });
	m_windows->add<InspectorWindow>(
		{ .id = "inspector", .title = "Inspector", .icon = "sliders-horizontal", .group = "Scene" });
	m_windows->add<ContentBrowserWindow>(
		{ .id = "content", .title = "Content", .icon = "folder-open", .group = "Scene" });
	m_windows->add<ConsoleWindow>({ .id = "console", .title = "Console", .icon = "terminal", .group = "Diagnostics" });
	m_windows->add<GameWindow>(
		{ .id = std::string(GameWindow::K_TYPE_ID), .title = "Game", .icon = "gamepad-2", .group = "Scene" });
	m_windows->add<TodoWindow>({ .id = "todo", .title = "Todo", .icon = "pencil" });
}

void EditorModule::add_window_type(EditorWindowType type) {
	if (m_windows) {
		add_window_command(type);
		m_windows->add(std::move(type));
		return;
	}
	m_pending_window_types.push_back(std::move(type));
}

void EditorModule::add_workspace(std::string name, DockLayoutDesc preset) {
	if (m_workspaces) {
		m_workspaces->add(std::move(name), std::move(preset));
		return;
	}
	m_pending_workspaces.emplace_back(std::move(name), std::move(preset));
}

void EditorModule::add_content_creator(ContentCreator creator) {
	if (m_context) {
		m_context->content_creators.push_back(std::move(creator));
		return;
	}
	m_pending_content_creators.push_back(std::move(creator));
}

void EditorModule::register_content_creators() {
	m_context->content_creators.push_back(folder_creator());
	m_context->content_creators.push_back(
		template_creator("Script", "file-code", "new_script.luau", "/resources/Engine/Templates/script.luau"));
	m_context->content_creators.push_back(
		template_creator("Material", "eclipse", "NewMaterial.aqmat", "/resources/Engine/Templates/material.aqmat"));
	m_context->content_creators.push_back(template_creator("Lit Shader", "file-code", "NewLitShader.slang",
														   "/resources/Engine/Templates/lit_shader.slang"));
	m_context->content_creators.push_back(template_creator("Unlit Shader", "file-code", "NewUnlitShader.slang",
														   "/resources/Engine/Templates/unlit_shader.slang"));
	m_context->content_creators.push_back(template_creator("Fullscreen Shader", "file-code",
														   "NewFullscreenShader.slang",
														   "/resources/Engine/Templates/fullscreen_shader.slang"));
	for (ContentCreator &creator : m_pending_content_creators) {
		m_context->content_creators.push_back(std::move(creator));
	}
	m_pending_content_creators.clear();
}

void EditorModule::register_workspaces() {
	using namespace DockLayout;

	m_workspaces->add(
		"Scene",
		layout(row({
			{ 0.18F, tabs({ "hierarchy" }) },
			{ 0.60F, column({ { 0.70F, tabs({ "viewport" }) }, { 0.30F, tabs({ "content", "console" }) } }) },
			{ 0.22F, tabs({ "inspector" }) },
		})));

	m_workspaces->add(
		"Layout",
		layout(row({
			{ 0.76F, tabs({ "viewport" }) },
			{ 0.24F, column({ { 0.40F, tabs({ "hierarchy" }) }, { 0.60F, tabs({ "inspector", "console" }) } }) },
		})));

	m_workspaces->add(
		"Debug",
		layout(column({
			{ 0.60F, row({ { 0.68F, tabs({ "viewport" }) }, { 0.32F, tabs({ "inspector", "hierarchy" }) } }) },
			{ 0.40F, tabs({ "console", "content" }) },
		})));

	m_workspaces->add(std::string(K_GAME_WORKSPACE),
					  layout(row({
						  { 0.18F, tabs({ "hierarchy" }) },
						  { 0.60F,
							column({ { 0.70F, tabs({ std::string(GameWindow::K_TYPE_ID) }) },
									 { 0.30F, tabs({ "console", "content" }) } }) },
						  { 0.22F, tabs({ "inspector" }) },
					  })));
}

void EditorModule::setup_editor_ui() {
	auto &editor_canvas = m_ui_host->get_canvas(UILayer::Editor);
	const auto &cfg = Config::get_preferences();
	LayoutLoader &layouts = m_ui_host->get_layout_loader();

	m_context =
		std::make_unique<EditorContext>(*m_engine, editor_canvas, layouts, *m_texture_cache, layout_dir(), m_project);
	m_context->set_camera(m_editor_camera.get());
	add_builtin_component_editors(m_context->component_editors);
	add_builtin_asset_types(m_context->asset_types);
	if (!m_thumbnails) {
		m_thumbnails =
			std::make_unique<AssetThumbnails>(m_engine->get_context(), *m_texture_cache, m_context->asset_types);
	}
	m_context->set_thumbnails(m_thumbnails.get());
	register_entity_slots();
	register_thumbnail_slots();
	register_content_creators();

	m_windows = std::make_unique<EditorWindows>(*m_context);
	register_windows();
	for (EditorWindowType &type : m_pending_window_types) {
		m_windows->add(std::move(type));
	}

	register_editor_commands();
	bind_default_shortcuts(m_context->shortcuts);
	apply_user_shortcuts(m_context->shortcuts, cfg.shortcuts);

	layouts.set_command_source(&m_context->commands, &m_context->shortcuts);

	m_pending_window_types.clear();

	const std::string shell_path = m_context->window_layout("editor");
	View *layout_root = layouts.build_file(shell_path, *editor_canvas.get_root(),
										   std::make_unique<EditorShell>(*m_context, *m_windows));
	m_shell = layout_root != nullptr ? find_behaviour<EditorShell>(*layout_root) : nullptr;
	if (m_shell == nullptr) {
		AQUILA_LOG_ERROR("EditorModule: failed to build the editor shell from {}", shell_path);
		return;
	}
	m_context->set_overlay_root(layout_root);

	m_dock_space = m_shell->get_dock_space();
	if (m_dock_space == nullptr) {
		AQUILA_LOG_ERROR("EditorModule: 'editor-dock' not found, check {}", shell_path);
		return;
	}

	m_windows->attach(*m_dock_space);
	m_context->shortcuts.set_scope_resolver(
		[this](std::string_view scope) { return m_windows && m_windows->is_scope_active(scope); });
	m_context->set_window_visibility(
		[this](std::string_view type_id) { return m_windows && m_windows->is_visible(type_id); });

	m_dock_manager = std::make_unique<DockWindowManager>(m_engine->get_window_host(), layouts);
	m_dock_manager->set_main_dock_space(m_dock_space);

	m_context->selection.on_changed.connect([this](Entity entity) { set_outlined_entity(entity); });
	m_engine->get_object_picking().on_picked.connect([this](Entity entity) {
		m_pending_pick = entity;
		Aquila::Foundation::FrameScheduler::get()->request_frame();
	});

	m_workspaces = std::make_unique<Workspaces>(*m_windows);
	register_workspaces();
	for (auto &[name, preset] : m_pending_workspaces) {
		m_workspaces->add(std::move(name), std::move(preset));
	}
	m_pending_workspaces.clear();
	if (TabBar *tabs = m_shell->get_workspace_tabs()) {
		m_workspaces->build_tabs(*tabs);
	}
	m_workspaces->activate("Scene");
	sync_play_controls();

	m_devtools = std::make_unique<Aquila::UI::DevTools::UIDevTools>(Aquila::UI::DevTools::UIDevToolsDesc{
		.target = editor_canvas,
		.host = m_engine->get_window_host(),
		.layouts = layouts,
	});
	m_devtools->attach(layout_root);

	editor_canvas.reload_styles();
}

void EditorModule::register_entity_slots() {
	AssetSlot::set_adapter(
		"entity",
		{
			.from_drag = [](const std::any &payload) -> Option<AssetPayload> {
				const auto *entity = std::any_cast<Entity>(&payload);
				if (entity == nullptr || !entity->exists() || !entity->has_component<MetadataComponent>()) {
					return std::nullopt;
				}
				return AssetPayload{
					.asset_path = entity->get_component<MetadataComponent>().get_id().to_string(),
					.asset_type = "entity",
					.display_name = entity->get_name(),
				};
			},
			.display_name =
				[this](const std::string &uuid) {
					const auto entity = m_engine->get_scene().get_entity_manager()->find_entity_by_uuid(
						Aquila::Foundation::UUID::from_string(uuid));
					return entity ? entity->get_name() : std::string("Missing entity");
				},
			.exists =
				[this](const std::string &uuid) {
					return m_engine->get_scene()
						.get_entity_manager()
						->find_entity_by_uuid(Aquila::Foundation::UUID::from_string(uuid))
						.has_value();
				},
		});
}

void EditorModule::register_thumbnail_slots() {
	for (const AssetType &type : m_context->asset_types.get_all()) {
		if (type.thumbnail != AssetThumbnail::None) {
			AssetSlot::set_adapter(
				type.id, { .thumbnail = [this](const std::string &path) { return m_thumbnails->get(path); } });
		}
	}
}

void EditorModule::reload_styles() {
	m_ui_host->get_layout_loader().styles().touch();
	for (Uint8 layer = 0; layer < static_cast<Uint8>(UILayer::Count); ++layer) {
		m_ui_host->get_canvas(static_cast<UILayer>(layer)).reload_styles();
	}
	if (m_dock_manager) {
		m_dock_manager->reload_styles();
	}
}

void EditorModule::request_layout_reload() {
	m_layout_reload_pending = true;
	Aquila::Foundation::FrameScheduler::get()->request_frame();
}

void EditorModule::reload_layouts() {
	m_ui_host->get_layout_loader().forget_documents();
	reload_styles();
	if (m_project_launcher) {
		m_project_launcher->rebuild();
	}
	if (m_settings_window) {
		m_settings_window->rebuild();
	}
	if (m_build_window) {
		if (const BuildWindow *build = build_window(); build != nullptr && build->is_busy()) {
			AQUILA_LOG_WARNING("EditorModule: the Build window is building, it keeps its old layout");
		} else {
			m_build_window->rebuild();
		}
	}
	if (m_windows) {
		m_windows->rebuild_all();
	}
	AQUILA_LOG_INFO("Layouts rebuilt");
	Aquila::Foundation::FrameScheduler::get()->request_frame();
}

bool EditorModule::handle_window_key(Aquila::Platform::Events::Event &event) {
	auto *key = dynamic_cast<Aquila::Platform::Events::KeyPressedEvent *>(&event);
	if (key == nullptr || key->is_repeat()) {
		return false;
	}
	const Commands no_commands;
	Shortcuts fallback(no_commands);
	const Shortcuts *shortcuts = m_context ? &m_context->shortcuts : nullptr;
	if (shortcuts == nullptr) {
		bind_default_shortcuts(fallback);
		apply_user_shortcuts(fallback, Config::get_preferences().shortcuts);
		shortcuts = &fallback;
	}
	const Aquila::Platform::Events::KeyChord chord = Aquila::Platform::Events::KeyChord::from(*key);
	for (const ShortcutBinding &binding : shortcuts->get_bindings(K_RELOAD_LAYOUTS)) {
		if (binding.chord == chord) {
			request_layout_reload();
			return true;
		}
	}
	return false;
}

void EditorModule::open_settings_window() {
	if (m_settings_window) {
		return;
	}

	m_settings_window = std::make_unique<BehaviourWindow>(
		m_ui_host->get_layout_loader(),
		[this]() -> Unique<WindowBehaviour> {
			auto settings = std::make_unique<SettingsWindow>(*m_context);
			settings->on_preferences_changed = [this] { preferences_edited(); };
			return settings;
		},
		860, 640);
	m_settings_window->set_event_hook(
		[this](Aquila::Platform::Events::Event &event) { return handle_window_key(event); });
	m_settings_window->open(m_engine->get_window_host(), "Aquila - Settings",
							[this] {
								m_settings_window.reset();
								save_settings_if_due(true);
							},
							{ 520.F, 360.F });
}

void EditorModule::open_build_window() {
	if (m_build_window) {
		return;
	}

	m_build_window = std::make_unique<BehaviourWindow>(
		m_ui_host->get_layout_loader(),
		[this]() -> Unique<WindowBehaviour> { return std::make_unique<BuildWindow>(*m_context); }, 720, 600);
	m_build_window->set_event_hook([this](Aquila::Platform::Events::Event &event) { return handle_window_key(event); });
	m_build_window->open(m_engine->get_window_host(), "Aquila - Build", [this] { m_build_window.reset(); },
						 { 480.F, 400.F });
}

BuildWindow *EditorModule::build_window() const {
	return m_build_window ? m_build_window->get_behaviour<BuildWindow>() : nullptr;
}

void EditorModule::set_ui_scale(F32 scale) {
	auto &prefs = Config::get_preferences();
	prefs.ui_scale = Math::clamp(scale, 0.5F, 2.F);
	prefs.save_to_file();
	m_applied_preferences.ui_scale = prefs.ui_scale;

	FontRegistry::set_ui_scale(prefs.ui_scale);
	reload_styles();
	AQUILA_LOG_INFO("EditorModule: interface scale {:.2f}", prefs.ui_scale);
}

void EditorModule::apply_font_settings() {
	const auto &prefs = Config::get_preferences();

	FontManager::get().reload(m_engine->get_context(), prefs.fonts);
	FontRegistry::set_ui_scale(prefs.ui_scale);
	reload_styles();
}

void EditorModule::apply_preferences(bool force) {
	const Config::EditorPreferences &prefs = Config::get_preferences();
	const Config::EditorPreferences &last = m_applied_preferences;

	if (!force && (prefs.fonts != last.fonts || prefs.ui_scale != last.ui_scale)) {
		apply_font_settings();
	}
	if (force || prefs.viewport != last.viewport) {
		Aquila::Rendering::RenderSettings &render = m_engine->get_render_pipeline().get_settings();
		render.show_grid = prefs.viewport.show_grid;
		render.show_sky = prefs.viewport.show_sky;
		render.show_shadows = prefs.viewport.show_shadows;
		render.show_outline = prefs.viewport.show_outline;
		render.realtime = prefs.viewport.realtime;
		render.tone_mapper = prefs.viewport.tone_mapper;
	}
	if (m_editor_camera && (force || prefs.navigation != last.navigation)) {
		m_editor_camera->set_move_speed(prefs.navigation.fly_speed);
		m_editor_camera->set_look_sensitivity(prefs.navigation.look_sensitivity);
		m_editor_camera->set_invert_y(prefs.navigation.invert_y);
		m_editor_camera->set_zoom_step(prefs.navigation.zoom_step);
		m_editor_camera->set_fov(prefs.navigation.fov);
	}
	if (m_context && (force || prefs.gizmos != last.gizmos)) {
		EditorTools &tools = m_context->tools;
		tools.set_snap_steps(prefs.gizmos.translate_snap, prefs.gizmos.rotate_snap, prefs.gizmos.scale_snap);
		tools.set_gizmo_size(prefs.gizmos.gizmo_size);
		tools.set_snapping(prefs.gizmos.snapping);
	}
	if ((force || prefs.advanced != last.advanced) && Aquila::Graphics::Shader::ShaderHotReload::is_alive()) {
		Aquila::Graphics::Shader::ShaderHotReload::get()->enable(prefs.advanced.shader_hot_reload);
	}

	m_applied_preferences = prefs;
	if (m_context) {
		m_context->on_settings_changed();
	}
	Aquila::Foundation::FrameScheduler::get()->request_frame();
}

void EditorModule::preferences_edited() {
	apply_preferences(false);
	m_preferences_save_due = Aquila::Foundation::now() + K_SETTINGS_SAVE_DELAY;
	Aquila::Foundation::FrameScheduler::get()->request_frame_in(K_SETTINGS_SAVE_SECONDS);
}

void EditorModule::write_project_settings(ProjectSettings settings) {
	if (settings == m_project.settings) {
		return;
	}
	m_project.settings = std::move(settings);
	if (m_context) {
		m_context->on_settings_changed();
	}
	m_project_save_due = Aquila::Foundation::now() + K_SETTINGS_SAVE_DELAY;
	Aquila::Foundation::FrameScheduler::get()->request_frame_in(K_SETTINGS_SAVE_SECONDS);
}

void EditorModule::update_luau_config() {
	if (!m_editor_entered || m_project.native_directory.empty()) {
		return;
	}
	const auto *api = m_engine->get_services().find<Aquila::Scripting::ScriptApi>();
	const std::pair<std::string, Uint64> current{ m_project.native_directory,
												  api != nullptr ? api->get_revision() : 0 };
	if (m_luau_config == current) {
		return;
	}
	m_luau_config = current;

	const std::filesystem::path resources = Aquila::Foundation::EnginePaths::resources_dir();
	const std::filesystem::path types = resources / "Engine" / "Types";
	const std::map<std::string, std::string> aliases =
		Aquila::Scripting::engine_luau_aliases((resources / "Engine" / "API").generic_string(), types.generic_string());
	Aquila::Scripting::update_luaurc(resources.string(), aliases);
	Aquila::Scripting::update_luaurc(m_project.native_directory, aliases);
	Aquila::Scripting::update_vscode_settings(m_project.native_directory, (types / "globals.d.luau").generic_string());
}

void EditorModule::save_settings_if_due(bool now) {
	const Aquila::Foundation::TimePoint current = Aquila::Foundation::now();
	if (m_preferences_save_due && (now || *m_preferences_save_due <= current)) {
		m_preferences_save_due.reset();
		Config::get_preferences().save_to_file();
	}
	if (m_project_save_due && (now || *m_project_save_due <= current)) {
		m_project_save_due.reset();
		if (m_project_manager && !m_project_manager->save_settings(m_project)) {
			AQUILA_LOG_ERROR("EditorModule: couldn't save the settings of '{}'", m_project.name);
		}
	}
}

void EditorModule::register_editor_commands() {
	Commands &commands = m_context->commands;

	const auto editing = [this] { return is_editing(); };
	commands.add({ .id = "scene.new", .title = "New scene", .run = [this] { new_empty_scene(); }, .enabled = editing });
	commands.add({
		.id = "scene.open",
		.title = "Open scene",
		.run = [this] { open_scene(); },
		.enabled = editing,
	});
	commands.add({ .id = "scene.save", .title = "Save scene", .run = [this] { save_scene(); }, .enabled = editing });
	commands.add(
		{ .id = "scene.save_as", .title = "Save scene as", .run = [this] { save_scene_as(); }, .enabled = editing });
	commands.add({
		.id = "scene.reset_demo",
		.title = "Reset demo scene",
		.run = [this] { reset_to_demo_scene(); },
		.enabled = editing,
	});

	commands.add({
		.id = "play.start",
		.title = "Play",
		.run = [this] { start_playing(); },
		.enabled = [this] { return m_engine->get_play_state() != PlayState::Playing; },
	});
	commands.add({
		.id = "play.pause",
		.title = "Pause",
		.run = [this] { toggle_pause(); },
		.enabled = [this] { return !is_editing(); },
	});
	commands.add({
		.id = "play.stop",
		.title = "Stop",
		.run = [this] { m_engine->stop(); },
		.enabled = [this] { return !is_editing(); },
	});

	commands.add({ .id = "editor.preferences", .title = "Preferences", .run = [this] { open_settings_window(); } });
	commands.add({ .id = "editor.quit", .title = "Quit", .run = [this] { m_engine->request_close(); } });

	commands.add({ .id = "window.open.build", .title = "Build", .run = [this] { open_build_window(); } });
	commands.add({
		.id = "project.build",
		.title = "Build project",
		.run =
			[this] {
				open_build_window();
				if (BuildWindow *build = build_window()) {
					build->start_build(false);
				}
			},
		.enabled = [this] { return build_window() == nullptr || !build_window()->is_busy(); },
	});
	commands.add({
		.id = std::string(K_RELOAD_LAYOUTS),
		.title = "Rebuild layouts",
		.run = [this] { request_layout_reload(); },
	});
	commands.add({
		.id = "editor.reload_styles",
		.title = "Reload styles",
		.run =
			[this] {
				reload_styles();
				AQUILA_LOG_INFO("Stylesheet reloaded");
			},
	});
	commands.add({
		.id = "console.clear",
		.title = "Clear console",
		.run =
			[this] {
				if (auto *console = m_windows->find<ConsoleWindow>()) {
					console->clear();
				}
			},
		.enabled = [this] { return m_windows->find<ConsoleWindow>() != nullptr; },
	});
	commands.add({
		.id = "ui.zoom_in",
		.title = "Zoom in",
		.run = [this] { set_ui_scale(Config::get_preferences().ui_scale + 0.05F); },
		.repeatable = true,
	});
	commands.add({
		.id = "ui.zoom_out",
		.title = "Zoom out",
		.run = [this] { set_ui_scale(Config::get_preferences().ui_scale - 0.05F); },
		.repeatable = true,
	});
	commands.add({ .id = "ui.zoom_reset", .title = "Reset zoom", .run = [this] { set_ui_scale(1.F); } });

	commands.add(
		{ .id = "workspace.next", .title = "Next workspace", .run = [this] { m_workspaces->activate_relative(1); } });
	commands.add({
		.id = "workspace.previous",
		.title = "Previous workspace",
		.run = [this] { m_workspaces->activate_relative(-1); },
	});
	commands.add(
		{ .id = "workspace.reset", .title = "Reset workspace", .run = [this] { m_workspaces->reset_active(); } });
	commands.add({
		.id = "view.status_bar",
		.title = "Status bar",
		.run =
			[this] {
				if (m_shell != nullptr) {
					m_shell->set_status_visible(!m_shell->is_status_visible());
				}
			},
	});

	commands.add({
		.id = "devtools.ui_inspector",
		.title = "UI inspector",
		.run = [this] { m_devtools->open_inspector_window(); },
		.enabled = [this] { return m_devtools != nullptr; },
	});
	commands.add({
		.id = "devtools.widget_gallery",
		.title = "Widget gallery",
		.run = [this] { m_devtools->open_widget_gallery(); },
		.enabled = [this] { return m_devtools != nullptr; },
	});

	commands.add({
		.id = "entity.create",
		.title = "Create entity",
		.run = [this] { m_context->create_entity(); },
	});
	commands.add({
		.id = "entity.delete",
		.title = "Delete entity",
		.run = [this] { m_context->destroy_entity(m_context->selection.get()); },
		.enabled = [this] { return m_context->selection.has(); },
	});

	for (const EditorWindowType &type : m_windows->get_types()) {
		add_window_command(type);
	}
}

void EditorModule::add_window_command(const EditorWindowType &type) {
	m_context->commands.add(
		{ .id = "window.open." + type.id, .title = type.title, .run = [this, id = type.id] { m_windows->open(id); } });
}

} // namespace Editor
