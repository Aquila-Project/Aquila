#include "UI/Windows/SettingsWindow.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/Platform/Events/KeyChord.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/FontManager.h"
#include "Aquila/UI/Core/Shortcuts.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/ReflectedPropertyGrid.h"
#include "Aquila/UI/Widgets/TabBar.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Core/EditorConfig.h"
#include "Core/EditorContext.h"
#include "Core/ProjectSettingsFields.h"
#include "UI/Components/SettingsSection.h"
#include "UI/Components/SettingsShortcutRow.h"
#include "UI/Widgets/ChordButton.h"

namespace Editor {

using namespace Aquila;
using namespace Aquila::UI::Core;
using Aquila::Foundation::contains_ignore_case;
using Aquila::Platform::Events::KeyChord;
using Aquila::Reflection::EnumOption;
using Aquila::Reflection::PropertyHints;
using Aquila::Reflection::PropertyMark;
using Aquila::Reflection::TypeBuilder;
using Aquila::Reflection::TypeInfo;
using Config::EditorPreferences;

namespace {

constexpr std::array<PropertyMark, 4> K_FLY_SPEED_MARKS = { {
	{ .value = 1.5F, .label = "Walking" },
	{ .value = 5.F, .label = "Running" },
	{ .value = 15.F, .label = "Driving" },
	{ .value = 60.F, .label = "Flying" },
} };

const std::vector<EnumOption> &tone_mapper_options() {
	static const std::vector<EnumOption> options = [] {
		std::vector<EnumOption> result;
		for (const Rendering::ToneMapper mapper : Rendering::TONE_MAPPERS) {
			result.push_back(
				{ .name = std::string(Rendering::tone_mapper_label(mapper)), .value = static_cast<Int32>(mapper) });
		}
		return result;
	}();
	return options;
}

std::string group_title(std::string_view command_id) {
	std::string group(command_id.substr(0, command_id.find('.')));
	if (!group.empty()) {
		group[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(group[0])));
	}
	return group;
}

} // namespace

SettingsWindow::SettingsWindow(EditorContext &context)
	: WindowBehaviour(context.window_layout("settings")), m_context(context) {}

SettingsWindow::~SettingsWindow() = default;

SettingsWindow::Page &SettingsWindow::add_page(Scope scope, std::string name, std::string icon,
											   std::string description) {
	Page &page = m_pages.emplace_back();
	page.scope = scope;
	page.name = std::move(name);
	page.icon = std::move(icon);
	page.description = std::move(description);
	page.type = std::make_unique<TypeInfo>(page.name);
	return page;
}

void SettingsWindow::add_editor_pages() {
	EditorPreferences &prefs = Config::get_preferences();

	Page &interface = add_page(Scope::Editor, "Interface", "palette", "Fonts and size of the whole editor.");
	interface.instance = &prefs;
	TypeBuilder<EditorPreferences>(*interface.type)
		.choice(
			"Main Font", [](EditorPreferences &p) -> std::string & { return p.fonts.main_family; }, m_font_families)
		.choice(
			"Mono Font", [](EditorPreferences &p) -> std::string & { return p.fonts.mono_family; }, m_font_families)
		.property("Interface Scale", &EditorPreferences::ui_scale,
				  PropertyHints{ .min = 0.5F, .max = 2.F, .speed = 0.01F, .precision = 2, .slider = true })
		.property(
			"Reopen Last Project", [](const EditorPreferences &p) { return p.startup.reopen_last_project; },
			[](EditorPreferences &p, bool value) { p.startup.reopen_last_project = value; },
			PropertyHints{ .toggle = true });
	interface.reset = [&prefs] {
		prefs.fonts = {};
		prefs.ui_scale = 1.F;
		prefs.startup = {};
	};

	using Viewport = Config::ViewportPreferences;
	Page &viewport = add_page(Scope::Editor, "Viewport", "eye",
							  "What the viewport shows when the editor starts. Changing one also applies it now.");
	viewport.instance = &prefs.viewport;
	TypeBuilder<Viewport>(*viewport.type)
		.property("Grid", &Viewport::show_grid, PropertyHints{ .toggle = true })
		.property("Sky", &Viewport::show_sky, PropertyHints{ .toggle = true })
		.property("Shadows", &Viewport::show_shadows, PropertyHints{ .toggle = true })
		.property("Selection Outline", &Viewport::show_outline, PropertyHints{ .toggle = true })
		.property("Update Continuously", &Viewport::realtime, PropertyHints{ .toggle = true })
		.property("Tone Mapping", &Viewport::tone_mapper)
		.options(tone_mapper_options())
		.property("Axis Widget", &Viewport::axis_widget, PropertyHints{ .toggle = true })
		.property("Selection Label", &Viewport::selection_label, PropertyHints{ .toggle = true })
		.property("Safe Frame", &Viewport::safe_frame, PropertyHints{ .toggle = true })
		.property("Render Status", &Viewport::render_status, PropertyHints{ .toggle = true })
		.property("Statistics", &Viewport::statistics, PropertyHints{ .toggle = true })
		.property("GPU Timings", &Viewport::gpu_timings, PropertyHints{ .toggle = true });
	viewport.reset = [&prefs] { prefs.viewport = {}; };

	using Navigation = Config::NavigationPreferences;
	Page &navigation = add_page(Scope::Editor, "Navigation", "video",
								"The editor camera: hold the right mouse button and use WASDQE to fly.");
	navigation.instance = &prefs.navigation;
	TypeBuilder<Navigation>(*navigation.type)
		.property("Fly Speed", &Navigation::fly_speed,
				  PropertyHints{ .min = 0.05F,
								 .max = 1000.F,
								 .precision = 2,
								 .unit = "m/s",
								 .logarithmic = true,
								 .slider_min = 0.5F,
								 .slider_max = 100.F,
								 .marks = K_FLY_SPEED_MARKS })
		.property("Look Sensitivity", &Navigation::look_sensitivity,
				  PropertyHints{ .min = 0.1F, .max = 4.F, .speed = 0.01F, .precision = 2, .slider = true })
		.property("Invert Y", &Navigation::invert_y, PropertyHints{ .toggle = true })
		.property("Zoom Step", &Navigation::zoom_step,
				  PropertyHints{ .min = 0.01F, .max = 10.F, .speed = 0.01F, .precision = 2, .unit = "m" })
		.property(
			"Field of View", &Navigation::fov,
			PropertyHints{ .min = 10.F, .max = 150.F, .speed = 0.5F, .precision = 1, .slider = true, .unit = "°" });
	navigation.reset = [&prefs] { prefs.navigation = {}; };

	using Gizmos = Config::GizmoPreferences;
	Page &gizmos = add_page(Scope::Editor, "Gizmos", "move-3d",
							"The transform gizmo and its snapping. Hold Ctrl while dragging to flip snapping.");
	gizmos.instance = &prefs.gizmos;
	TypeBuilder<Gizmos>(*gizmos.type)
		.property("Snap by Default", &Gizmos::snapping, PropertyHints{ .toggle = true })
		.property("Move Step", &Gizmos::translate_snap,
				  PropertyHints{ .min = 0.001F, .max = 100.F, .speed = 0.01F, .precision = 3, .unit = "m" })
		.property("Rotate Step", &Gizmos::rotate_snap,
				  PropertyHints{ .min = 0.1F, .max = 180.F, .speed = 0.5F, .precision = 1, .unit = "°" })
		.property("Scale Step", &Gizmos::scale_snap,
				  PropertyHints{ .min = 0.001F, .max = 10.F, .speed = 0.01F, .precision = 3 })
		.property(
			"Gizmo Size", &Gizmos::gizmo_size,
			PropertyHints{ .min = 32.F, .max = 256.F, .speed = 1.F, .precision = 0, .slider = true, .unit = "px" });
	gizmos.reset = [&prefs] { prefs.gizmos = {}; };

	using Play = Config::PlayPreferences;
	Page &play = add_page(Scope::Editor, "Play", "play", "What Play and Stop do with the workspaces.");
	play.instance = &prefs.play;
	TypeBuilder<Play>(*play.type)
		.property("Switch to Game View", &Play::switch_to_game_view, PropertyHints{ .toggle = true })
		.property("Return After Stop", &Play::return_after_stop, PropertyHints{ .toggle = true });
	play.reset = [&prefs] { prefs.play = {}; };

	using Browser = Config::ContentBrowserPreferences;
	Page &browser = add_page(Scope::Editor, "Content Browser", "folder", "How the content browser shows files.");
	browser.instance = &prefs.content_browser;
	TypeBuilder<Browser>(*browser.type)
		.property(
			"Thumbnail Size", &Browser::thumbnail_size,
			PropertyHints{ .min = 32.F, .max = 160.F, .speed = 1.F, .precision = 0, .slider = true, .unit = "px" })
		.property("File Extensions", &Browser::show_extensions, PropertyHints{ .toggle = true });
	browser.reset = [&prefs] { prefs.content_browser = {}; };

	Page &shortcuts = add_page(Scope::Editor, "Shortcuts", "keyboard",
							   "Click a shortcut and press the new keys. Backspace removes it, Escape cancels.");
	shortcuts.shortcuts = true;
	shortcuts.reset = [this, &prefs] {
		for (const auto &[command, chords] : prefs.shortcuts) {
			m_context.shortcuts.reset_chords(command);
		}
		prefs.shortcuts.clear();
	};

	using Advanced = Config::AdvancedPreferences;
	Page &advanced = add_page(Scope::Editor, "Advanced", "wrench", "For working on the engine itself.");
	advanced.instance = &prefs.advanced;
	TypeBuilder<Advanced>(*advanced.type)
		.property("Shader Hot Reload", &Advanced::shader_hot_reload, PropertyHints{ .toggle = true })
		.read_only("Preferences File", [](const Advanced &) {
			const std::string &path = Config::get_preferences().preference_file_path;
			return Platform::Filesystem::VirtualFileSystem::get()->to_native_path(path).value_or(path);
		});
	advanced.reset = [&prefs] { prefs.advanced = {}; };
}

void SettingsWindow::add_project_pages() {
	const ProjectInfo &project = m_context.project();

	Page &application = add_page(Scope::Project, "Application", "package", "What the project is and where it starts.");
	application.instance = &m_project;
	TypeBuilder<ProjectSettings>(*application.type)
		.read_only("Name", [this](const ProjectSettings &) { return m_context.project().name; })
		.read_only("Folder", [this](const ProjectSettings &) { return m_context.project().native_directory; });
	add_startup_scene_field(*application.type, project);
	add_icon_field(*application.type, project);
	application.reset = [this] {
		const ProjectSettings defaults;
		m_project.startup_scene = defaults.startup_scene;
		m_project.icon = defaults.icon;
	};

	Page &player = add_page(Scope::Project, "Player", "gamepad-2", "The window a built game opens.");
	player.instance = &m_project;
	TypeBuilder<ProjectSettings>(*player.type).property("Window Title", &ProjectSettings::window_title);
	add_window_size_fields(*player.type);
	TypeBuilder<ProjectSettings>(*player.type)
		.property("Start Maximized", &ProjectSettings::start_maximized, PropertyHints{ .toggle = true });
	player.reset = [this] {
		const ProjectSettings defaults;
		m_project.window_title = defaults.window_title;
		m_project.window_width = defaults.window_width;
		m_project.window_height = defaults.window_height;
		m_project.start_maximized = defaults.start_maximized;
	};

	Page &rendering =
		add_page(Scope::Project, "Rendering", "sun", "How the game draws, in builds and in the editor's Game view.");
	rendering.instance = &m_project;
	TypeBuilder<ProjectSettings>(*rendering.type)
		.property("Tone Mapping", &ProjectSettings::tone_mapper)
		.options(tone_mapper_options())
		.property("Shadows", &ProjectSettings::shadows, PropertyHints{ .toggle = true });
	rendering.reset = [this] {
		const ProjectSettings defaults;
		m_project.tone_mapper = defaults.tone_mapper;
		m_project.shadows = defaults.shadows;
	};

	Page &build = add_page(Scope::Project, "Build", "folder-open",
						   "Where Build writes the game. Empty uses the Builds folder next to the editor.");
	build.instance = &m_project;
	add_build_output_fields(*build.type);
	build.reset = [this] { m_project.build_output.clear(); };
}

void SettingsWindow::define_behaviour() {
	m_project = m_context.project().settings;
	m_font_families = FontManager::available_families();
	const Config::FontSettings &fonts = Config::get_preferences().fonts;
	for (const std::string &family : { fonts.main_family, fonts.mono_family }) {
		if (std::ranges::find(m_font_families, family) == m_font_families.end()) {
			m_font_families.push_back(family);
		}
	}
	add_editor_pages();
	add_project_pages();

	m_sidebar = get("settings-nav");
	m_content = get("settings-content");
	m_footer_note = get<Label>("settings-footer-note");
	m_scope_tabs = get<TabBar>("settings-scopes");
	m_search = get<TextInput>("settings-search");
	if (m_sidebar == nullptr || m_content == nullptr || m_footer_note == nullptr || m_scope_tabs == nullptr ||
		m_search == nullptr) {
		return;
	}
	m_scope_tabs->on_selected.connect([this](int index) { select_scope(static_cast<Scope>(index)); });
	m_search->on_changed.connect([this](const std::string &text) {
		m_query = text;
		request_rebuild();
	});
	if (auto *close_button = get<Button>("settings-close")) {
		close_button->on_click.connect([this] { close(); });
	}

	listen(m_context.on_settings_changed, [this] {
		if (m_context.project().settings != m_project) {
			m_project = m_context.project().settings;
			for (const Unique<ReflectedPropertyGrid> &grid : m_grids) {
				grid->refresh();
			}
		}
	});

	select_scope(Scope::Editor);
}

void SettingsWindow::select_scope(Scope scope) {
	m_scope = scope;
	m_scope_tabs->set_active(static_cast<int>(scope));
	m_footer_note->set_text(
		scope == Scope::Editor ? std::string("Saved for you in editor_preferences.json, for every project.")
							   : "Saved in " + m_context.project().name + "/project.aqproject and used by its builds.");
	request_rebuild();
}

void SettingsWindow::select_page(Usize index) {
	m_selected[static_cast<Usize>(m_scope)] = index;
	if (m_search != nullptr && !m_query.empty()) {
		m_search->set_text(std::string());
		m_query.clear();
	}
	request_rebuild();
}

void SettingsWindow::request_rebuild() {
	m_rebuild_pending = true;
	const Canvas *canvas = root().get_canvas();
	Foundation::FrameScheduler::get()->request_frame(canvas != nullptr ? canvas->get_frame_target() : nullptr);
}

void SettingsWindow::rebuild() {
	m_rebuild_pending = false;
	m_grids.clear();
	m_filtered_types.clear();
	m_content->clear_children();
	rebuild_sidebar();

	if (m_query.empty()) {
		Usize ordinal = 0;
		for (Page &page : m_pages) {
			if (page.scope != m_scope) {
				continue;
			}
			if (ordinal++ == m_selected[static_cast<Usize>(m_scope)]) {
				show_page(page, std::string(), true);
				break;
			}
		}
		return;
	}

	bool found = false;
	for (Page &page : m_pages) {
		if (page.scope == m_scope) {
			const Usize before = m_content->get_children().size();
			show_page(page, contains_ignore_case(page.name, m_query) ? std::string() : m_query, false);
			found = found || m_content->get_children().size() > before;
		}
	}
	if (!found) {
		create("SettingsEmpty", *m_content, { .attributes = { { "text", "Nothing matches \"" + m_query + "\"." } } });
	}
}

void SettingsWindow::rebuild_sidebar() {
	m_sidebar->clear_children();
	Usize ordinal = 0;
	for (const Page &page : m_pages) {
		if (page.scope != m_scope) {
			continue;
		}
		const Usize index = ordinal++;
		auto *item = dynamic_cast<Button *>(
			create("SettingsNavItem", *m_sidebar, { .attributes = { { "label", page.name }, { "icon", page.icon } } }));
		if (item == nullptr) {
			continue;
		}
		item->set_class("settings-nav-item-active",
						m_query.empty() && index == m_selected[static_cast<Usize>(m_scope)]);
		item->on_click.connect([this, index] { select_page(index); });
	}
}

void SettingsWindow::show_page(Page &page, const std::string &query, bool full) {
	if (page.shortcuts) {
		auto *section = create<SettingsSection>("SettingsSection", *m_content);
		if (section == nullptr) {
			return;
		}
		section->show(page.name, page.description, full, full ? "Reset All Shortcuts" : "");
		if (PropertyGrid *grid = section->get_grid()) {
			grid->set_hidden(true);
		}
		if (!show_shortcuts(*section, query)) {
			m_content->remove_child(&section->root());
			return;
		}
		if (Button *reset = section->get_reset(); reset != nullptr && full) {
			reset->on_click.connect([this, &page] {
				page.reset();
				edited(page);
				request_rebuild();
			});
		}
		return;
	}

	const TypeInfo *shown = page.type.get();
	if (!query.empty()) {
		auto filtered = std::make_unique<TypeInfo>(page.name);
		for (const Reflection::Property &property : page.type->get_properties()) {
			if (contains_ignore_case(property.name, query)) {
				filtered->add(property);
			}
		}
		if (filtered->get_properties().empty()) {
			return;
		}
		shown = filtered.get();
		m_filtered_types.push_back(std::move(filtered));
	}

	auto *section = create<SettingsSection>("SettingsSection", *m_content);
	if (section == nullptr || section->get_grid() == nullptr) {
		return;
	}
	const bool resettable = full && query.empty() && page.reset;
	section->show(page.name, page.description, full, resettable ? "Reset to Defaults" : "");
	auto reflected =
		std::make_unique<ReflectedPropertyGrid>(*section->get_grid(), *shown, &m_context.engine().get_context());
	reflected->bind(page.instance, [this, &page] { edited(page); });
	m_grids.push_back(std::move(reflected));

	if (Button *reset = section->get_reset(); reset != nullptr && resettable) {
		reset->on_click.connect([this, &page] {
			page.reset();
			edited(page);
			for (const Unique<ReflectedPropertyGrid> &shown : m_grids) {
				shown->refresh();
			}
		});
	}
}

bool SettingsWindow::show_shortcuts(SettingsSection &section, const std::string &query) {
	Shortcuts &shortcuts = m_context.shortcuts;
	Commands &editor_commands = m_context.commands;
	std::vector<const Command *> commands;
	for (const Command &command : editor_commands.all()) {
		if (contains_ignore_case(command.title, query) || contains_ignore_case(command.id, query) ||
			contains_ignore_case(shortcuts.get_label(command.id), query)) {
			commands.push_back(&command);
		}
	}
	std::ranges::stable_sort(commands, {}, [](const Command *command) { return group_title(command->id); });

	std::string group;
	for (const Command *command : commands) {
		if (const std::string title = group_title(command->id); title != group) {
			group = title;
			section.add("SettingsShortcutGroup", { .attributes = { { "title", group } } });
		}

		View *row_view = section.add("SettingsShortcutRow");
		auto *row = row_view != nullptr ? find_behaviour<SettingsShortcutRow>(*row_view) : nullptr;
		if (row == nullptr) {
			continue;
		}
		const std::string id = command->id;
		row->show(command->title, shortcuts.get_label(id), shortcuts.has_override(id));
		if (ChordButton *chord = row->get_chord()) {
			chord->on_chord = [this, id](Option<KeyChord> pressed) {
				std::vector<KeyChord> chords;
				if (pressed) {
					chords.push_back(*pressed);
				}
				m_context.shortcuts.set_chords(id, chords);
				std::vector<std::string> &saved = Config::get_preferences().shortcuts[id];
				saved.clear();
				for (const KeyChord &bound : chords) {
					saved.push_back(bound.to_string());
				}
				if (on_preferences_changed) {
					on_preferences_changed();
				}
				request_rebuild();
			};
		}
		if (Button *reset = row->get_reset()) {
			reset->on_click.connect([this, id] {
				m_context.shortcuts.reset_chords(id);
				Config::get_preferences().shortcuts.erase(id);
				if (on_preferences_changed) {
					on_preferences_changed();
				}
				request_rebuild();
			});
		}

		std::vector<std::string> clashes;
		for (const Aquila::UI::Core::ShortcutBinding &binding : shortcuts.get_bindings(id)) {
			for (const Command &other : editor_commands.all()) {
				if (other.id == id) {
					continue;
				}
				for (const Aquila::UI::Core::ShortcutBinding &theirs : shortcuts.get_bindings(other.id)) {
					if (theirs.chord == binding.chord && theirs.scope == binding.scope &&
						std::ranges::find(clashes, other.title) == clashes.end()) {
						clashes.push_back(other.title);
					}
				}
			}
		}
		if (!clashes.empty()) {
			std::string text = "Also used by ";
			for (Usize i = 0; i < clashes.size(); ++i) {
				text += (i == 0 ? "" : ", ") + clashes[i];
			}
			section.add("SettingsShortcutClash", { .attributes = { { "text", std::move(text) } } });
		}
	}
	return !commands.empty();
}

void SettingsWindow::update(F32) {
	if (m_rebuild_pending) {
		rebuild();
	}
}

void SettingsWindow::edited(const Page &page) {
	if (page.scope == Scope::Project) {
		m_context.set_project_settings(m_project);
		return;
	}
	if (on_preferences_changed) {
		on_preferences_changed();
	}
}

} // namespace Editor
