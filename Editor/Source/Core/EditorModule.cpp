#include "Core/EditorModule.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Core/EditorContext.h"
#include "Core/EditorWindows.h"
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

	Events::EventDispatcher dispatcher(event);

	if (m_editor_camera) {
		m_editor_camera->on_event(event);
	}

	if (from_main && m_devtools && m_devtools->on_event(event)) {
		return;
	}

	dispatcher.dispatch<Events::KeyPressedEvent>([this](Events::KeyPressedEvent &e) {
		if (e.get_key_code() == Events::KeyCode::F5) {
			m_layout_loader.load_file(Config::get_preferences().ui.layout_path);
			AQUILA_LOG_INFO("Editor layout reloaded");
			return true;
		}

		if (e.get_key_code() == Events::KeyCode::F6) {
			load_styles();
			AQUILA_LOG_INFO("Stylesheet reloaded");
			return true;
		}
		return false;
	});

	if (from_main) {
		m_ui_host->on_event(event);
	}

	Events::EventDispatcher post(event);
	post.dispatch<Events::KeyPressedEvent>([this](Events::KeyPressedEvent &e) {
		if (e.get_mods() != Events::MODIFIER_CONTROL) {
			return false;
		}
		const auto &prefs = Config::get_preferences();
		switch (e.get_key_code()) {
		case Events::KeyCode::Equal:
		case Events::KeyCode::KeypadAdd:
			set_ui_scale(prefs.ui_scale + 0.05F);
			return true;
		case Events::KeyCode::Minus:
		case Events::KeyCode::KeypadSubtract:
			set_ui_scale(prefs.ui_scale - 0.05F);
			return true;
		case Events::KeyCode::Num0:
			set_ui_scale(1.F);
			return true;
		default:
			return false;
		}
	});
	post.dispatch<Events::KeyPressedEvent>([this, source_canvas](Events::KeyPressedEvent &e) {
		if (!m_context || e.get_mods() != 0 ||
			Aquila::Platform::Input::is_mouse_button_pressed(Events::MouseButton::Right)) {
			return false;
		}
		if (Aquila::UI::Core::view_is<Aquila::UI::Core::TextInput>(source_canvas->get_focused_view())) {
			return false;
		}
		switch (e.get_key_code()) {
		case Events::KeyCode::W:
			m_context->tools.set_tool(TransformTool::Translate);
			return true;
		case Events::KeyCode::E:
			m_context->tools.set_tool(TransformTool::Rotate);
			return true;
		case Events::KeyCode::R:
			m_context->tools.set_tool(TransformTool::Scale);
			return true;
		case Events::KeyCode::X:
			return m_context->tools.key_handler && m_context->tools.key_handler(TransformKey::X);
		case Events::KeyCode::Y:
			return m_context->tools.key_handler && m_context->tools.key_handler(TransformKey::Y);
		case Events::KeyCode::Z:
			return m_context->tools.key_handler && m_context->tools.key_handler(TransformKey::Z);
		case Events::KeyCode::Enter:
		case Events::KeyCode::KeypadEnter:
			return m_context->tools.key_handler && m_context->tools.key_handler(TransformKey::Confirm);
		case Events::KeyCode::Escape:
			return m_context->tools.key_handler && m_context->tools.key_handler(TransformKey::Cancel);
		default:
			return false;
		}
	});
	post.dispatch<Events::KeyPressedEvent>([this, source_canvas](Events::KeyPressedEvent &e) {
		if (e.get_key_code() != Events::KeyCode::S || e.get_mods() != Events::MODIFIER_SHIFT) {
			return false;
		}
		if (Aquila::UI::Core::view_is<Aquila::UI::Core::TextInput>(source_canvas->get_focused_view())) {
			return false;
		}
		if (Aquila::Platform::Input::is_mouse_button_pressed(Events::MouseButton::Right) ||
			Aquila::Platform::Input::is_mouse_button_pressed(Events::MouseButton::Left)) {
			return false;
		}
		auto *inspector = m_windows ? m_windows->find<InspectorWindow>() : nullptr;
		if (inspector == nullptr) {
			return false;
		}
		inspector->open_add_search(Aquila::Platform::Input::get_mouse_position());
		return true;
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

	m_layout_loader.register_command("entity.create", [this] {
		auto entity = m_engine->get_scene().get_entity_manager()->create_entity("New Entity");
		m_context->selection.on_entity_created(entity);
		m_context->selection.select(entity);
	});
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

void EditorModule::wire_main_menu(Aquila::UI::Core::View *layout_root) {
	using Aquila::UI::Core::PopupMenu;

	auto *menu_button = layout_root->find_by_id<Aquila::UI::Core::Button>("topbar-menu");
	if (menu_button == nullptr) {
		return;
	}

	auto popup = std::make_unique<PopupMenu>();
	popup->set_submenu_icon(m_context->icon("chevron-right"));
	m_main_menu = dynamic_cast<PopupMenu *>(layout_root->add_child(std::move(popup)));

	PopupMenu *file_menu = m_main_menu->add_submenu("File", m_context->icon("folder-open"));
	file_menu->add_item("New scene", "Ctrl+N", m_context->icon("layers-plus"), [this] { new_empty_scene(); });
	file_menu->add_item("Open scene", "Ctrl+O", m_context->icon("folder-open"),
						[] { AQUILA_LOG_INFO("EditorModule: opening scenes is not available yet"); });
	file_menu->add_separator();
	file_menu->add_item("Quit", "Ctrl+X", m_context->icon("x"), [this] { m_engine->request_close(); });

	PopupMenu *edit_menu = m_main_menu->add_submenu("Edit", m_context->icon("pencil"));
	edit_menu->add_item("Preferences", "Ctrl+,", m_context->icon("settings"), [this] { open_settings_window(); });
	edit_menu->add_separator();
	edit_menu->add_item("Reset demo scene", {}, m_context->icon("refresh-cw"), [this] { reset_to_demo_scene(); });

	PopupMenu *window_menu = m_main_menu->add_submenu("Window", m_context->icon("panel-left"));
	for (const EditorWindowType &type : m_windows->get_types()) {
		window_menu->add_item(type.title, {}, m_context->icon(type.icon),
							  [this, id = type.id] { m_windows->open(id); });
	}
	window_menu->add_separator();
	window_menu->add_item("Reset workspace", {}, m_context->icon("refresh-cw"),
						  [this] { m_workspaces->reset_active(); });
	window_menu->add_item("Status bar", {}, m_context->icon("info"), [this] {
		if (m_status_bar) {
			m_status_bar->set_visible(!m_status_bar->is_visible());
		}
	});
	window_menu->add_separator();
	window_menu->add_item("UI inspector", {}, m_context->icon("bug"), [this] { m_devtools->open_inspector_window(); });
	window_menu->add_item("Widget gallery", {}, m_context->icon("layers"),
						  [this] { m_devtools->open_widget_gallery(m_engine->get_context(), m_texture_cache.get()); });

	menu_button->on_click.connect([this, menu_button] {
		const Rect rect = menu_button->get_absolute_rect();
		m_main_menu->open_at({ rect.position.x, rect.position.y + rect.size.y + 4.F });
	});
}

} // namespace Editor
