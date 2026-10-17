#include "Core/EditorModule.h"
#include "Aquila/Foundation/Math/Math.h"
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
#include "UI/EditorWindows/HierarchyWindow.h"
#include "UI/EditorWindows/InspectorWindow.h"
#include "UI/EditorWindows/ViewportWindow.h"
#include "UI/Panels/StatusBar.h"
#include "UI/Windows/ProjectLauncher.h"
#include "UI/Windows/SettingsWindow.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Platform/Events/InputEvent.h"
#include "Aquila/Platform/Events/KeyChord.h"
#include "Aquila/Platform/Input.h"
#include "Aquila/Rendering/Systems/ObjectPickingSystem.h"
#include "Aquila/Scene/Components/OutlineComponent.h"
#include "Aquila/Scene/DefaultScenes.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/UI/Core/DockLayoutBuilder.h"
#include "Aquila/UI/Core/DockWindowManager.h"
#include "Aquila/UI/Core/FontRegistry.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/UIHost.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/ColorPicker.h"
#include "Aquila/UI/Widgets/DockSpace.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Aquila/UI/Widgets/TextInput.h"

#include "Aquila/Scene/Scene.h"
#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Foundation/FrameScheduler.h"

namespace Editor {

using namespace Aquila;
using namespace Aquila::SceneManagement;
using namespace Aquila::SceneManagement::Components;
using namespace Aquila::Application;
namespace Events = Aquila::Platform::Events;

EditorModule::EditorModule() = default;

EditorModule::~EditorModule() = default;

void EditorModule::on_attach(EngineContext &engine) {
	m_engine = &engine;

	Window &main_window = m_engine->get_window();
	m_ui_host = std::make_unique<Aquila::UI::Core::UIHost>(Aquila::UI::Core::UIHostDesc{
		.overlay_renderer = m_engine->get_overlay_renderer(),
		.width = main_window.get_width(),
		.height = main_window.get_height(),
		.clipboard_get = [&main_window] { return main_window.get_clipboard_text(); },
		.clipboard_set = [&main_window](const std::string &text) { main_window.set_clipboard_text(text); },
	});

	Config::get_preferences().load_from_file();
	Aquila::UI::Core::FontRegistry::set_ui_scale(Config::get_preferences().ui_scale);

	Aquila::UI::Core::FontManager::get().initialize(m_engine->get_context(), Config::get_preferences().fonts);

	m_project_manager = std::make_unique<ProjectManager>();
	m_texture_cache = std::make_unique<Aquila::UI::Core::TextureCache>(m_engine->get_context(),
																	   Config::get_preferences().ui.resources_path);

	open_project_launcher();
}

void EditorModule::open_project_launcher() {
	m_project_launcher = std::make_unique<ProjectLauncher>();
	m_project_launcher->build(m_project_manager.get(), m_texture_cache.get(), 900, 600,
							  Config::get_preferences().ui.style_paths);
	m_project_launcher->on_project_ready = [this](const ProjectInfo &project) {
		m_pending_project = project;
		Aquila::Foundation::FrameScheduler::get()->request_frame();
	};

	m_launcher_window = Aquila::Rendering::open_content_window(m_engine->get_window_host(), *m_project_launcher, 900,
															   600, "Aquila - Projects", [this] {
																   m_project_launcher.reset();
																   m_launcher_window = nullptr;
																   if (!m_editor_entered) {
																	   m_engine->request_close();
																   }
															   });
}

void EditorModule::enter_editor(const ProjectInfo &project) {
	AQUILA_LOG_INFO("EditorModule: opening project '{}' ({})", project.name, project.directory);

	m_editor_entered = true;
	if (m_launcher_window != nullptr) {
		m_engine->get_window_host().request_close(m_launcher_window);
	}

	m_engine->get_window().show();
	m_editor_camera = std::make_unique<Aquila::Rendering::CameraController>();
	setup_editor_ui();
}

void EditorModule::on_detach() {
	if (m_windows) {
		m_windows->shutdown();
	}
	m_dock_manager.reset();
	m_settings_window.reset();
	m_project_launcher.reset();
	m_devtools.reset();

	m_ui_host.reset();
	m_workspaces.reset();
	m_windows.reset();
	m_context.reset();
	m_texture_cache.reset();
	Aquila::UI::Core::FontManager::get().shutdown();
}

void EditorModule::on_pre_render(F32 delta_time) {
	if (m_pending_project) {
		const ProjectInfo project = *m_pending_project;
		m_pending_project.reset();
		enter_editor(project);
	}

	if (m_pending_pick && m_context) {
		const Entity picked = *m_pending_pick;
		m_pending_pick.reset();
		if (picked.is_valid()) {
			m_context->selection.select(picked);
		} else {
			m_context->selection.clear();
		}
	}

	if (m_status_bar && m_status_bar->is_visible()) {
		const bool has_selection = m_outlined_entity.is_valid() && m_outlined_entity.exists();
		m_status_bar->update(m_statistics.collect(m_engine->get_scene(), m_outlined_entity),
							 has_selection ? m_outlined_entity.get_name() : std::string());
	}

	if (m_windows) {
		m_windows->update(delta_time);
	}
	m_ui_host->update(delta_time);

	m_engine->get_window().set_cursor(m_ui_host->get_active_cursor());
}

Aquila::UI::Core::Canvas *EditorModule::canvas_for(Events::EventSource source) const {
	if (source == m_engine->get_window_host().get_main_window()) {
		return &m_ui_host->get_canvas(Aquila::UI::Core::UILayer::Editor);
	}
	return m_dock_manager ? m_dock_manager->find_canvas(source) : nullptr;
}

void EditorModule::on_event(Events::Event &event) {
	Aquila::UI::Core::Canvas *source_canvas = canvas_for(event.get_source());
	if (source_canvas == nullptr) {
		return;
	}
	const bool from_main = event.get_source() == m_engine->get_window_host().get_main_window();

	if (m_editor_camera) {
		m_editor_camera->on_event(event);
	}

	if (from_main && m_devtools && m_devtools->on_event(event)) {
		return;
	}

	if (from_main) {
		m_ui_host->on_event(event);
	}

	Events::EventDispatcher dispatcher(event);
	dispatcher.dispatch<Events::KeyPressedEvent>([this, source_canvas](Events::KeyPressedEvent &e) {
		if (!m_context) {
			return false;
		}
		const bool typing = Aquila::UI::Core::view_is<Aquila::UI::Core::TextInput>(source_canvas->get_focused_view());
		return m_context->shortcuts.handle(Events::KeyChord::from(e), e.is_repeat(), typing);
	});
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

void EditorModule::on_resize(Uint32, Uint32) {
	if (m_context) {
		m_context->on_render_output_changed();
	}
}

void EditorModule::on_render_resize(Uint32, Uint32) {
	if (m_context) {
		m_context->on_render_output_changed();
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
	SceneManagement::spawn_default_camera(m_engine->get_scene(), window_aspect());
	scene_replaced();
	AQUILA_LOG_INFO("EditorModule: new empty scene");
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
}

void EditorModule::add_window_type(EditorWindowType type) {
	if (m_windows) {
		add_window_command(type);
		m_windows->add(std::move(type));
		return;
	}
	m_pending_window_types.push_back(std::move(type));
}

void EditorModule::add_workspace(std::string name, Aquila::UI::Core::DockLayoutDesc preset) {
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
	for (ContentCreator &creator : m_pending_content_creators) {
		m_context->content_creators.push_back(std::move(creator));
	}
	m_pending_content_creators.clear();
}

void EditorModule::register_workspaces() {
	using namespace Aquila::UI::Core::DockLayout;

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
}

void EditorModule::setup_editor_ui() {
	auto &editor_canvas = m_ui_host->get_canvas(Aquila::UI::Core::UILayer::Editor);
	const auto &cfg = Config::get_preferences();

	load_styles();

	m_layout_loader.register_font("regular", Aquila::UI::Core::FontManager::get().get_font("regular"));
	m_layout_loader.register_texture_cache(m_texture_cache.get());
	m_layout_loader.register_widget(
		"ColorPicker", [this](std::string_view, Aquila::UI::Text::FontAtlas *) -> Unique<Aquila::UI::Core::View> {
			return std::make_unique<Aquila::UI::Core::ColorPicker>(m_engine->get_context());
		});

	const std::string layout_dir = cfg.ui.layout_path.substr(0, cfg.ui.layout_path.find_last_of('/') + 1);
	m_context =
		std::make_unique<EditorContext>(*m_engine, editor_canvas, m_layout_loader, *m_texture_cache, layout_dir);
	m_context->set_camera(m_editor_camera.get());
	register_content_creators();
	bind_default_shortcuts(m_context->shortcuts);

	m_layout_loader.register_command("entity.create", [this] { m_context->shortcuts.execute("entity.create"); });
	m_layout_loader.register_command("console.clear", [this] {
		if (auto *console = m_windows->find<ConsoleWindow>()) {
			console->clear();
		}
	});

	auto root = m_layout_loader.load_file(cfg.ui.layout_path);
	if (!root) {
		AQUILA_LOG_ERROR("EditorModule: failed to load editor layout from {}", cfg.ui.layout_path);
		return;
	}

	Aquila::UI::Core::View *layout_root = editor_canvas.get_root()->add_child(std::move(root));
	m_context->set_overlay_root(layout_root);

	m_dock_space = layout_root->find_by_id<Aquila::UI::Core::DockSpace>("editor-dock");
	if (m_dock_space == nullptr) {
		AQUILA_LOG_ERROR("EditorModule: 'editor-dock' not found, check editor.aqlayout");
		return;
	}

	m_windows = std::make_unique<EditorWindows>(*m_context);
	register_windows();
	for (EditorWindowType &type : m_pending_window_types) {
		m_windows->add(std::move(type));
	}
	m_pending_window_types.clear();
	m_windows->attach(*m_dock_space);
	m_context->shortcuts.set_scope_resolver(
		[this](std::string_view scope) { return m_windows && m_windows->is_scope_active(scope); });

	m_dock_manager =
		std::make_unique<Aquila::UI::Core::DockWindowManager>(m_engine->get_window_host(), cfg.ui.style_paths);
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
	if (auto *tabs = layout_root->find_by_id("workspace-tabs")) {
		m_workspaces->build_tabs(*tabs);
	}
	m_workspaces->activate("Scene");

	register_commands();
	wire_main_menu(layout_root);
	m_status_bar = std::make_unique<StatusBar>();
	m_status_bar->build(layout_root);

	if (auto *settings = layout_root->find_by_id<Aquila::UI::Core::Button>("topbar-settings")) {
		settings->on_click.connect([this] { open_settings_window(); });
	}
	if (auto *reset = layout_root->find_by_id<Aquila::UI::Core::Button>("topbar-reset")) {
		reset->on_click.connect([this] { m_workspaces->reset_active(); });
	}

	m_devtools = std::make_unique<Aquila::UI::DevTools::UIDevTools>(Aquila::UI::DevTools::UIDevToolsDesc{
		.target = editor_canvas,
		.host = m_engine->get_window_host(),
		.style_paths = cfg.ui.style_paths,
	});
	m_devtools->attach(layout_root);

	editor_canvas.reload_styles();
}

void EditorModule::load_styles() {
	const auto &style_paths = Config::get_preferences().ui.style_paths;
	for (Uint8 layer = 0; layer < static_cast<Uint8>(Aquila::UI::Core::UILayer::Count); ++layer) {
		auto &canvas = m_ui_host->get_canvas(static_cast<Aquila::UI::Core::UILayer>(layer));
		Aquila::UI::StyleParser::load_files(style_paths, canvas.get_style_sheet());
	}
	reload_styles();
}

void EditorModule::reload_styles() {
	for (Uint8 layer = 0; layer < static_cast<Uint8>(Aquila::UI::Core::UILayer::Count); ++layer) {
		m_ui_host->get_canvas(static_cast<Aquila::UI::Core::UILayer>(layer)).reload_styles();
	}
	if (m_dock_manager) {
		m_dock_manager->reload_styles();
	}
}

void EditorModule::open_settings_window() {
	if (m_settings_window) {
		return;
	}

	m_settings_window = std::make_unique<SettingsWindow>();
	m_settings_window->build(m_texture_cache.get(), 560, 640, Config::get_preferences().ui.style_paths);
	m_settings_window->on_applied = [this] { apply_font_settings(); };

	const Aquila::Rendering::RenderWindowId window =
		Aquila::Rendering::open_content_window(m_engine->get_window_host(), *m_settings_window, 560, 640,
											   "Aquila - Settings", [this] { m_settings_window.reset(); });
	m_settings_window->on_request_close = [this, window] { m_engine->get_window_host().request_close(window); };
}

void EditorModule::set_ui_scale(F32 scale) {
	auto &prefs = Config::get_preferences();
	prefs.ui_scale = Math::clamp(scale, 0.5F, 2.F);
	prefs.save_to_file();

	Aquila::UI::Core::FontRegistry::set_ui_scale(prefs.ui_scale);
	reload_styles();
	AQUILA_LOG_INFO("EditorModule: interface scale {:.2f}", prefs.ui_scale);
}

void EditorModule::apply_font_settings() {
	const auto &prefs = Config::get_preferences();

	Aquila::UI::Core::FontManager::get().reload(m_engine->get_context(), prefs.fonts);
	Aquila::UI::Core::FontRegistry::set_ui_scale(prefs.ui_scale);
	reload_styles();
}

void EditorModule::register_commands() {
	Aquila::UI::Core::Shortcuts &shortcuts = m_context->shortcuts;

	const auto editing = [] { return true; };
	shortcuts.add_command(
		{ .id = "scene.new", .title = "New scene", .run = [this] { new_empty_scene(); }, .enabled = editing });
	shortcuts.add_command({
		.id = "scene.reset_demo",
		.title = "Reset demo scene",
		.run = [this] { reset_to_demo_scene(); },
		.enabled = editing,
	});


	shortcuts.add_command(
		{ .id = "editor.preferences", .title = "Preferences", .run = [this] { open_settings_window(); } });
	shortcuts.add_command({ .id = "editor.quit", .title = "Quit", .run = [this] { m_engine->request_close(); } });

	shortcuts.add_command({
		.id = "editor.reload_layout",
		.title = "Reload layout",
		.run =
			[this] {
				m_layout_loader.load_file(Config::get_preferences().ui.layout_path);
				AQUILA_LOG_INFO("Editor layout reloaded");
			},
	});
	shortcuts.add_command({
		.id = "editor.reload_styles",
		.title = "Reload styles",
		.run =
			[this] {
				load_styles();
				AQUILA_LOG_INFO("Stylesheet reloaded");
			},
	});

	shortcuts.add_command({
		.id = "ui.zoom_in",
		.title = "Zoom in",
		.run = [this] { set_ui_scale(Config::get_preferences().ui_scale + 0.05F); },
		.repeatable = true,
	});
	shortcuts.add_command({
		.id = "ui.zoom_out",
		.title = "Zoom out",
		.run = [this] { set_ui_scale(Config::get_preferences().ui_scale - 0.05F); },
		.repeatable = true,
	});
	shortcuts.add_command({ .id = "ui.zoom_reset", .title = "Reset zoom", .run = [this] { set_ui_scale(1.F); } });

	shortcuts.add_command(
		{ .id = "workspace.next", .title = "Next workspace", .run = [this] { m_workspaces->activate_relative(1); } });
	shortcuts.add_command({
		.id = "workspace.previous",
		.title = "Previous workspace",
		.run = [this] { m_workspaces->activate_relative(-1); },
	});
	shortcuts.add_command(
		{ .id = "workspace.reset", .title = "Reset workspace", .run = [this] { m_workspaces->reset_active(); } });
	shortcuts.add_command({
		.id = "view.status_bar",
		.title = "Status bar",
		.run =
			[this] {
				if (m_status_bar) {
					m_status_bar->set_visible(!m_status_bar->is_visible());
				}
			},
	});

	shortcuts.add_command({
		.id = "devtools.ui_inspector",
		.title = "UI inspector",
		.run = [this] { m_devtools->open_inspector_window(); },
		.enabled = [this] { return m_devtools != nullptr; },
	});
	shortcuts.add_command({
		.id = "devtools.widget_gallery",
		.title = "Widget gallery",
		.run = [this] { m_devtools->open_widget_gallery(m_engine->get_context(), m_texture_cache.get()); },
		.enabled = [this] { return m_devtools != nullptr; },
	});

	shortcuts.add_command({
		.id = "entity.create",
		.title = "Create entity",
		.run =
			[this] {
				auto entity = m_engine->get_scene().get_entity_manager()->create_entity("New Entity");
				m_context->selection.on_entity_created(entity);
				m_context->selection.select(entity);
			},
	});
	shortcuts.add_command({
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
	m_context->shortcuts.add_command(
		{ .id = "window.open." + type.id, .title = type.title, .run = [this, id = type.id] { m_windows->open(id); } });
}

void EditorModule::wire_main_menu(Aquila::UI::Core::View *layout_root) {
	using Aquila::UI::Core::PopupMenu;

	auto *menu_button = layout_root->find_by_id<Aquila::UI::Core::Button>("topbar-menu");
	if (menu_button == nullptr) {
		return;
	}

	auto popup = std::make_unique<PopupMenu>();
	popup->set_submenu_icon(m_context->icon("chevron-right"));
	m_main_menu = dynamic_cast<PopupMenu *>(layout_root->add_child(std::move(popup)));

	const auto add = [this](PopupMenu *menu, std::string_view command_id, std::string_view icon) {
		const Aquila::UI::Core::Command *command = m_context->shortcuts.find_command(command_id);
		if (command == nullptr) {
			AQUILA_LOG_WARNING("EditorModule: menu command '{}' is not registered", command_id);
			return;
		}
		menu->add_item(command->title, m_context->shortcuts.get_label(command_id), m_context->icon(icon),
					   [this, id = std::string(command_id)] { m_context->shortcuts.execute(id); });
	};

	PopupMenu *file_menu = m_main_menu->add_submenu("File", m_context->icon("folder-open"));
	file_menu->add_item("Open scene", "Ctrl+O", m_context->icon("folder-open"),
						[] { AQUILA_LOG_INFO("EditorModule: opening scenes is not available yet"); });
	add(file_menu, "scene.new", "layers-plus");
	file_menu->add_separator();
	add(file_menu, "editor.quit", "x");

	PopupMenu *edit_menu = m_main_menu->add_submenu("Edit", m_context->icon("pencil"));
	add(edit_menu, "editor.preferences", "settings");
	edit_menu->add_separator();
	add(edit_menu, "scene.reset_demo", "refresh-cw");

	PopupMenu *window_menu = m_main_menu->add_submenu("Window", m_context->icon("panel-left"));
	for (const EditorWindowType &type : m_windows->get_types()) {
		add(window_menu, "window.open." + type.id, type.icon);
	}
	window_menu->add_separator();
	add(window_menu, "workspace.reset", "refresh-cw");
	add(window_menu, "view.status_bar", "info");
	window_menu->add_separator();
	add(window_menu, "devtools.ui_inspector", "bug");
	add(window_menu, "devtools.widget_gallery", "layers");

	menu_button->on_click.connect([this, menu_button] {
		const Rect rect = menu_button->get_absolute_rect();
		m_main_menu->open_at({ rect.position.x, rect.position.y + rect.size.y + 4.F });
	});
}

} // namespace Editor
