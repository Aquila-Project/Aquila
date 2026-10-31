#include "UI/Windows/ProjectLauncher.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/Platform/Input.h"
#include "Aquila/Platform/Process.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Clickable.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Aquila/UI/Widgets/TabBar.h"
#include "Aquila/UI/Widgets/TextInput.h"

namespace Editor {

using namespace Aquila;
using namespace Aquila::UI::Core;

namespace {

constexpr int K_CARDS_PER_ROW = 3;
constexpr int K_THUMB_PALETTE = 6;
constexpr const char *K_WIKI_URL = "https://github.com/Aquila-Project/Aquila/wiki";

std::string relative_time(Uint64 timestamp) {
	if (timestamp == 0) {
		return "never";
	}
	const auto now = std::chrono::system_clock::now().time_since_epoch();
	const Uint64 seconds_now = static_cast<Uint64>(std::chrono::duration_cast<std::chrono::seconds>(now).count());
	const Uint64 elapsed = seconds_now > timestamp ? seconds_now - timestamp : 0;
	auto plural = [](Uint64 value, const char *unit) {
		return std::format("{} {}{} ago", value, unit, value == 1 ? "" : "s");
	};
	if (elapsed < 60) {
		return "just now";
	}
	if (elapsed < 3600) {
		return plural(elapsed / 60, "minute");
	}
	if (elapsed < 86400) {
		return plural(elapsed / 3600, "hour");
	}
	if (elapsed < 86400 * 30) {
		return plural(elapsed / 86400, "day");
	}
	if (elapsed < 86400 * 365) {
		return plural(elapsed / (86400 * 30), "month");
	}
	return plural(elapsed / (86400 * 365), "year");
}

std::string initial_of(const std::string &name) {
	for (const char c : name) {
		if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
			return std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
		}
	}
	return "?";
}

} // namespace

ProjectLauncher::ProjectLauncher(std::string layout_path, ProjectManager &project_manager)
	: WindowBehaviour(std::move(layout_path)), m_project_manager(project_manager) {}

ProjectLauncher::~ProjectLauncher() = default;

void ProjectLauncher::define_behaviour() {
	if (auto *version = get<Label>("launcher-version")) {
		version->set_text(std::format("v{}.{}.{}", AQUILA_VERSION_MAJOR, AQUILA_VERSION_MINOR, AQUILA_VERSION_PATCH));
	}
	m_rail = get<TabBar>("launcher-rail");
	m_pages[static_cast<int>(Page::Recent)] = get("launcher-page-recent");
	m_pages[static_cast<int>(Page::New)] = get("launcher-page-new");
	m_pages[static_cast<int>(Page::Learn)] = get("launcher-page-learn");
	if (m_rail == nullptr) {
		return;
	}
	m_rail->on_selected.connect([this](int page) { show_page(static_cast<Page>(page)); });

	wire_recent_page();
	wire_new_page();
	if (auto *wiki = get<Clickable>("launcher-wiki")) {
		wiki->on_click.connect([] {
			if (!Platform::open_url(K_WIKI_URL)) {
				AQUILA_LOG_ERROR("ProjectLauncher: could not open {}", K_WIKI_URL);
			}
		});
	}

	m_context_menu = get<PopupMenu>("launcher-menu");
	if (m_context_menu != nullptr) {
		m_context_menu->set_action("open", [this] {
			if (m_context_project) {
				open_project(*m_context_project);
			}
		});
		m_context_menu->set_action("rename", [this] {
			if (m_context_project) {
				begin_action(ActionMode::Rename, *m_context_project);
			}
		});
		m_context_menu->set_action("delete", [this] {
			if (m_context_project) {
				begin_action(ActionMode::Delete, *m_context_project);
			}
		});
	}

	select_template(ProjectTemplate::Empty);
	refresh_list();
	show_page(Page::Recent);
}

void ProjectLauncher::wire_recent_page() {
	m_recent_count = get<Label>("launcher-count");
	m_grid_scroll = get("launcher-grid-scroll");
	m_grid_host = get("launcher-grid");
	m_empty_state = get("launcher-empty");
	if (auto *empty_button = get<Button>("launcher-empty-create")) {
		empty_button->on_click.connect([this] { show_page(Page::New); });
	}

	m_action_bar = get("launcher-action-bar");
	m_action_label = get<Label>("launcher-action-label");
	m_action_input = get<TextInput>("launcher-action-input");
	m_action_confirm = get<Button>("launcher-action-confirm");
	if (m_action_input != nullptr) {
		m_action_input->on_submit.connect([this](const std::string &) { confirm_action(); });
	}
	if (m_action_confirm != nullptr) {
		m_action_confirm->on_click.connect([this] { confirm_action(); });
	}
	if (auto *cancel = get<Button>("launcher-action-cancel")) {
		cancel->on_click.connect([this] { end_action(); });
	}
}

void ProjectLauncher::wire_new_page() {
	if (View *templates = get("launcher-templates")) {
		for (const ProjectTemplate option : { ProjectTemplate::Empty, ProjectTemplate::Sandbox }) {
			auto *card = dynamic_cast<Clickable *>(
				create("LauncherTemplate", *templates,
					   { .attributes = { { "name", ProjectManager::template_name(option) },
										 { "description", ProjectManager::template_description(option) } } }));
			if (card == nullptr) {
				continue;
			}
			card->on_click.connect([this, option] { select_template(option); });
			m_template_cards[static_cast<int>(option)] = card;
		}
	}

	m_name_input = get<TextInput>("launcher-name");
	m_location_label = get<Label>("launcher-location");
	m_error_label = get<Label>("launcher-error");
	m_create_button = get<Button>("launcher-create");
	if (m_name_input != nullptr) {
		m_name_input->on_changed.connect([this](const std::string &) { update_new_form(); });
		m_name_input->on_submit.connect([this](const std::string &) { create_project(); });
	}
	if (m_create_button != nullptr) {
		m_create_button->on_click.connect([this] { create_project(); });
	}
	if (auto *back = get<Button>("launcher-back")) {
		back->on_click.connect([this] { show_page(Page::Recent); });
	}
}

void ProjectLauncher::show_page(Page page) {
	m_page = page;
	if (m_rail != nullptr) {
		m_rail->set_active(static_cast<int>(page));
	}
	for (int i = 0; i < 3; ++i) {
		if (m_pages[i] != nullptr) {
			m_pages[i]->set_class("hidden", i != static_cast<int>(page));
		}
	}
	if (page == Page::Recent) {
		refresh_list();
	}
	if (page == Page::New && m_name_input != nullptr) {
		update_new_form();
		m_name_input->request_focus();
	}
}

void ProjectLauncher::refresh_list() {
	if (m_grid_host == nullptr || m_grid_scroll == nullptr || m_empty_state == nullptr || m_recent_count == nullptr) {
		return;
	}

	m_grid_host->clear_children();

	const std::vector<ProjectInfo> projects = m_project_manager.list();
	const bool has_projects = !projects.empty();
	m_empty_state->set_class("hidden", has_projects);
	m_grid_scroll->set_class("hidden", !has_projects);
	m_recent_count->set_text(
		has_projects ? std::format("{} project{}", projects.size(), projects.size() == 1 ? "" : "s") : "");

	if (!has_projects) {
		return;
	}

	View *row = nullptr;
	int in_row = 0;
	auto next_row = [&]() -> View & {
		if (row == nullptr || in_row == K_CARDS_PER_ROW) {
			row = create("LauncherRow", *m_grid_host);
			in_row = 0;
		}
		++in_row;
		return row != nullptr ? *row : *m_grid_host;
	};
	auto add_card = [&](const std::string &name, const std::string &meta, const std::string &letter,
						const std::string &palette) {
		return dynamic_cast<Clickable *>(create(
			"LauncherCard", next_row(),
			{ .attributes = { { "name", name }, { "meta", meta }, { "letter", letter }, { "palette", palette } } }));
	};

	if (auto *new_card = add_card("New project", "Start from a template", "+", "new")) {
		new_card->on_click.connect([this] { show_page(Page::New); });
	}

	for (const ProjectInfo &project : projects) {
		const std::size_t scene_count = project.scenes.size();
		const std::string meta = std::format("Opened {}  ·  {} scene{}", relative_time(project.last_activity()),
											 scene_count, scene_count == 1 ? "" : "s");
		const std::size_t palette = std::hash<std::string>{}(project.name) % K_THUMB_PALETTE;
		auto *card = add_card(project.name, meta, initial_of(project.name), std::to_string(palette));
		if (card == nullptr) {
			continue;
		}
		card->on_click.connect([this, project] { open_project(project); });
		card->on_context_menu.connect([this, project](Vec2 pos) {
			m_context_project = project;
			if (m_context_menu != nullptr) {
				m_context_menu->open_at(pos);
			}
		});
	}

	while (row != nullptr && in_row < K_CARDS_PER_ROW) {
		create("LauncherSpacer", *row);
		++in_row;
	}
}

void ProjectLauncher::begin_action(ActionMode mode, const ProjectInfo &project) {
	if (m_action_bar == nullptr || m_action_label == nullptr || m_action_input == nullptr ||
		m_action_confirm == nullptr) {
		return;
	}
	m_action_mode = mode;
	m_context_project = project;
	m_action_bar->set_class("hidden", false);
	const bool renaming = mode == ActionMode::Rename;
	m_action_input->set_class("hidden", !renaming);
	m_action_confirm->set_class("launcher-danger", !renaming);
	m_action_confirm->set_text(renaming ? "Rename" : "Delete");
	if (renaming) {
		m_action_label->set_text("Rename");
		m_action_input->set_text(project.name);
		m_action_input->request_focus();
	} else {
		m_action_label->set_text(std::format("Delete '{}' and all of its files? This cannot be undone.", project.name));
	}
}

void ProjectLauncher::end_action() {
	m_action_mode = ActionMode::None;
	if (m_action_bar != nullptr) {
		m_action_bar->set_class("hidden", true);
	}
}

void ProjectLauncher::confirm_action() {
	if (!m_context_project || m_action_input == nullptr || m_action_label == nullptr) {
		end_action();
		return;
	}

	ProjectInfo project = *m_context_project;
	if (m_action_mode == ActionMode::Rename) {
		if (!m_project_manager.rename(project, m_action_input->get_text())) {
			m_action_label->set_text("Enter a name for the project");
			return;
		}
	} else if (m_action_mode == ActionMode::Delete) {
		if (!m_project_manager.remove(project)) {
			m_action_label->set_text("Could not delete the project folder");
			return;
		}
	}

	m_context_project.reset();
	end_action();
	refresh_list();
}

void ProjectLauncher::select_template(ProjectTemplate project_template) {
	m_template = project_template;
	for (int i = 0; i < 2; ++i) {
		if (m_template_cards[i] != nullptr) {
			m_template_cards[i]->set_class("launcher-template-selected", i == static_cast<int>(project_template));
		}
	}
}

void ProjectLauncher::update_new_form() {
	if (m_name_input == nullptr || m_location_label == nullptr || m_error_label == nullptr ||
		m_create_button == nullptr) {
		return;
	}
	const std::string &name = m_name_input->get_text();
	const std::string error = m_project_manager.validate_name(name);
	const bool has_name = !name.empty();

	const std::string safe = Aquila::Foundation::to_file_name(name);
	m_location_label->set_text(safe.empty() ? "" : std::format("Will be created in Projects/{}", safe));
	m_error_label->set_text(has_name ? error : "");
	m_error_label->set_class("hidden", !has_name || error.empty());
	m_create_button->set_class("launcher-primary-disabled", !error.empty());
}

void ProjectLauncher::open_project(ProjectInfo project) {
	m_project_manager.touch(project);
	if (on_project_ready) {
		on_project_ready(project);
	}
}

void ProjectLauncher::create_project() {
	if (m_name_input == nullptr || m_error_label == nullptr) {
		return;
	}

	const std::string name = m_name_input->get_text();
	update_new_form();
	if (!m_project_manager.validate_name(name).empty()) {
		return;
	}

	if (Option<ProjectInfo> created = m_project_manager.create(name, m_template)) {
		m_name_input->set_text("");
		open_project(*created);
	} else {
		m_error_label->set_text("Could not create the project. Check the log for details.");
		m_error_label->set_class("hidden", false);
	}
}

} // namespace Editor
