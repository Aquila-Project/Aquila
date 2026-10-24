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
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/ReflectedPropertyGrid.h"
#include "Aquila/UI/Widgets/ScrollView.h"
#include "Aquila/UI/Widgets/TabBar.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Core/EditorConfig.h"
#include "Core/EditorContext.h"
#include "Core/ProjectSettingsFields.h"

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

class ChordButton final : public Button {
  public:
	explicit ChordButton(std::string label) : Button(label), m_label(std::move(label)) {
		on_click.connect([this] { start_capture(); });
	}

	Delegate<void(Option<KeyChord>)> on_chord;

	void on_key_press(Platform::KeyCode key, int mods) override {
		if (!m_capturing) {
			return;
		}
		if (key == Platform::KeyCode::Escape) {
			stop_capture();
			return;
		}
		if (key == Platform::KeyCode::Backspace || key == Platform::KeyCode::Delete) {
			finish(std::nullopt);
			return;
		}
		const KeyChord chord{ .key = key, .mods = mods & Platform::Events::MODIFIER_MASK };
		if (!chord.is_valid() || chord.is_modifier_only()) {
			return;
		}
		finish(chord);
	}

	void on_focus_lost() override {
		Button::on_focus_lost();
		stop_capture();
	}

	[[nodiscard]] std::string_view get_type_name() const override { return "ChordButton"; }

  private:
	void start_capture() {
		m_capturing = true;
		set_text("Press a shortcut...");
		add_class("settings-chord-capturing");
		request_focus();
	}

	void stop_capture() {
		if (!m_capturing) {
			return;
		}
		m_capturing = false;
		set_text(m_label);
		remove_class("settings-chord-capturing");
	}

	void finish(Option<KeyChord> chord) {
		stop_capture();
		Delegate<void(Option<KeyChord>)> callback = on_chord;
		if (callback) {
			callback(chord);
		}
	}

	std::string m_label;
	bool m_capturing = false;
};

} // namespace

SettingsWindow::SettingsWindow(EditorContext &context) : m_context(context) {}

SettingsWindow::~SettingsWindow() {
	m_context.on_settings_changed.disconnect(m_settings_connection);
}

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

void SettingsWindow::build(Uint32 width, Uint32 height, const std::vector<std::string> &style_paths) {
	create_canvas(width, height, style_paths);

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

	auto *root = get_canvas()->get_root();
	root->set_id("settings-root");
	root->add_class("settings-root");
	build_header(*root);

	auto *body = root->add_child<View>();
	body->add_class("settings-body");
	auto *sidebar_scroll = body->add_child<ScrollView>();
	sidebar_scroll->add_class("settings-sidebar");
	m_sidebar = sidebar_scroll->add_child<View>();
	m_sidebar->add_class("settings-nav");

	auto *main = body->add_child<View>();
	main->add_class("settings-main");
	auto *scroll = main->add_child<ScrollView>();
	scroll->add_class("settings-scroll");
	m_content = scroll->add_child<View>();
	m_content->add_class("settings-content");

	build_footer(*root);

	m_settings_connection = m_context.on_settings_changed.connect([this] {
		if (m_context.project().settings != m_project) {
			m_project = m_context.project().settings;
			for (const Unique<ReflectedPropertyGrid> &grid : m_grids) {
				grid->refresh();
			}
		}
	});

	select_scope(Scope::Editor);
	get_canvas()->reload_styles();
}

void SettingsWindow::build_header(View &root) {
	auto *header = root.add_child<View>();
	header->add_class("settings-header");
	auto *icon = header->add_child<Aquila::UI::Core::Image>(m_context.icon("settings"));
	icon->add_class("settings-header-icon");
	header->add_child<Label>(std::string("Settings"))->add_class("settings-title");

	m_scope_tabs = header->add_child<TabBar>("settings-scope");
	m_scope_tabs->add_class("settings-scopes");
	m_scope_tabs->add_tab("Editor")->set_tooltip("Your preferences, the same in every project");
	m_scope_tabs->add_tab("Project")->set_tooltip("Saved in this project, shipped with its builds");
	m_scope_tabs->on_selected.connect([this](int index) { select_scope(static_cast<Scope>(index)); });

	auto *search = header->add_child<View>();
	search->add_class("settings-search");
	search->add_child<Aquila::UI::Core::Image>(m_context.icon("search"))->add_class("settings-search-icon");
	m_search = search->add_child<TextInput>();
	m_search->add_class("settings-search-input");
	m_search->set_placeholder("Search settings");
	m_search->on_changed.connect([this](const std::string &text) {
		m_query = text;
		request_rebuild();
	});
}

void SettingsWindow::build_footer(View &root) {
	auto *footer = root.add_child<View>();
	footer->add_class("settings-footer");
	m_footer_note = footer->add_child<Label>(std::string());
	m_footer_note->add_class("settings-footer-note");
	auto *close = footer->add_child<Button>(std::string("Close"));
	close->add_class("settings-btn");
	close->on_click.connect([this] { this->close(); });
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
	Foundation::FrameScheduler::get()->request_frame(get_canvas()->get_frame_target());
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
		m_content->add_child<Label>("Nothing matches \"" + m_query + "\".")->add_class("settings-empty");
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
		auto *item = m_sidebar->add_child<Button>(page.name);
		item->add_class("settings-nav-item");
		item->set_icon(m_context.icon(page.icon));
		item->set_class("settings-nav-item-active",
						m_query.empty() && index == m_selected[static_cast<Usize>(m_scope)]);
		item->on_click.connect([this, index] { select_page(index); });
	}
}

void SettingsWindow::show_page(Page &page, const std::string &query, bool full) {
	if (page.shortcuts) {
		auto *section = m_content->add_child<View>();
		section->add_class("settings-section");
		const Usize before = section->get_children().size();
		section->add_child<Label>(page.name)->add_class(full ? "settings-page-title" : "settings-section-title");
		if (full) {
			section->add_child<Label>(page.description)->add_class("settings-page-description");
		}
		show_shortcuts(*section, query);
		if (section->get_children().size() <= before + (full ? 2 : 1)) {
			m_content->remove_child(section);
			return;
		}
		if (full) {
			auto *reset = section->add_child<Button>(std::string("Reset All Shortcuts"));
			reset->add_class("settings-reset");
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

	auto *section = m_content->add_child<View>();
	section->add_class("settings-section");
	section->add_child<Label>(page.name)->add_class(full ? "settings-page-title" : "settings-section-title");
	if (full && !page.description.empty()) {
		section->add_child<Label>(page.description)->add_class("settings-page-description");
	}
	auto *grid = section->add_child<PropertyGrid>(150.F);
	grid->set_split(true);
	grid->add_class("settings-grid");
	auto reflected = std::make_unique<ReflectedPropertyGrid>(*grid, *shown, &m_context.engine().get_context());
	reflected->bind(page.instance, [this, &page] { edited(page); });
	m_grids.push_back(std::move(reflected));
	if (!query.empty()) {
		return;
	}

	if (full && page.reset) {
		auto *reset = section->add_child<Button>(std::string("Reset to Defaults"));
		reset->add_class("settings-reset");
		reset->on_click.connect([this, &page] {
			page.reset();
			edited(page);
			for (const Unique<ReflectedPropertyGrid> &shown : m_grids) {
				shown->refresh();
			}
		});
	}
}

void SettingsWindow::show_shortcuts(View &host, const std::string &query) {
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
			host.add_child<Label>(group)->add_class("settings-shortcut-group");
		}

		auto *row = host.add_child<View>();
		row->add_class("settings-shortcut-row");
		row->add_child<Label>(command->title)->add_class("settings-shortcut-title");

		const std::string label = shortcuts.get_label(command->id);
		auto *chord = row->add_child<ChordButton>(label.empty() ? std::string("Not set") : label);
		chord->add_class("settings-chord");
		chord->set_class("settings-chord-empty", label.empty());
		chord->set_tooltip("Click, then press the new shortcut");
		const std::string id = command->id;
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

		auto *reset = row->add_child<Button>();
		reset->add_class("settings-chord-reset");
		reset->set_icon(m_context.icon("undo-2"));
		reset->set_tooltip("Back to the default shortcut");
		reset->set_hidden(!shortcuts.has_override(id));
		reset->on_click.connect([this, id] {
			m_context.shortcuts.reset_chords(id);
			Config::get_preferences().shortcuts.erase(id);
			if (on_preferences_changed) {
				on_preferences_changed();
			}
			request_rebuild();
		});

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
			host.add_child<Label>(text)->add_class("settings-shortcut-clash");
		}
	}
}

void SettingsWindow::on_update(F32) {
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
