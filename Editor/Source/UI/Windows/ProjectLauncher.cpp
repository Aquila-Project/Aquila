#include "UI/Windows/ProjectLauncher.h"
#include "Core/FileDialog.h"

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
#include "Aquila/UI/Widgets/TextInput.h"

namespace Editor {

using namespace Aquila;
using namespace Aquila::UI::Core;

namespace {

constexpr const char *K_WIKI_URL = "https://github.com/Aquila-Project/Aquila/wiki";
constexpr const char *K_REPO_URL = "https://github.com/Aquila-Project/Aquila/";

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

} // namespace

ProjectLauncher::ProjectLauncher(std::string layout_path, ProjectManager &project_manager)
	: WindowBehaviour(std::move(layout_path)), m_project_manager(project_manager) {}

ProjectLauncher::~ProjectLauncher() = default;

void ProjectLauncher::define_behaviour() {
	if (auto *version = get<Label>("launcher-version")) {
		version->set_text(std::format("Engine {}", AQUILA_VERSION_STRING));
	}
	m_home_page = get("launcher-page-home");
	m_new_page = get("launcher-page-new");

	wire_home_page();
	wire_new_page();
	if (auto *docs = get<Clickable>("launcher-docs")) {
		docs->on_click.connect([] {
			if (!Platform::open_url(K_WIKI_URL)) {
				AQUILA_LOG_ERROR("ProjectLauncher: could not open {}", K_WIKI_URL);
			}
		});
	}

	if (auto *git = get<Clickable>("launcher-git")) {
		git->on_click.connect([] {
			if (!Platform::open_url(K_REPO_URL)) {
				AQUILA_LOG_ERROR("ProjectLauncher: could not open {}", K_REPO_URL);
			}
		});
	}

	m_context_menu = get<PopupMenu>("launcher-menu");
	m_external_menu = get<PopupMenu>("launcher-external-menu");
	wire_menu(m_context_menu);
	wire_menu(m_external_menu);

	show_page(m_page);
}

void ProjectLauncher::take_state(WindowBehaviour &previous) {
	auto *launcher = dynamic_cast<ProjectLauncher *>(&previous);
	if (launcher == nullptr) {
		return;
	}
	m_page = launcher->m_page;
	if (launcher->m_name_input != nullptr) {
		m_restored_name = launcher->m_name_input->get_text();
	}
	if (launcher->m_location_input != nullptr) {
		m_restored_location = launcher->m_location_input->get_text();
	}
}

void ProjectLauncher::wire_home_page() {
	if (auto *new_tile = get<Clickable>("launcher-new-tile")) {
		new_tile->on_click.connect([this] { show_page(Page::New); });
	}
	if (auto *open_tile = get<Clickable>("launcher-open-tile")) {
		open_tile->on_click.connect([this] { browse_for_project(); });
	}
	m_open_error = get<Label>("launcher-open-error");
	m_recent_scroll = get("launcher-recent-scroll");
	m_recent_host = get("launcher-recent");
	m_empty_state = get("launcher-empty");

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
	m_name_input = get<TextInput>("launcher-name");
	m_name_error = get<Label>("launcher-name-error");
	m_location_input = get<TextInput>("launcher-location");
	m_location_error = get<Label>("launcher-location-error");
	m_preview = get<Label>("launcher-preview");
	m_error_label = get<Label>("launcher-error");
	m_create_button = get<Button>("launcher-create");
	if (m_location_input != nullptr) {
		m_location_input->set_text(m_restored_location.value_or(m_project_manager.get_native_root()));
	}
	for (TextInput *input : { m_name_input, m_location_input }) {
		if (input != nullptr) {
			input->on_changed.connect([this](const std::string &) { update_new_form(); });
			input->on_submit.connect([this](const std::string &) { create_project(); });
		}
	}
	if (m_create_button != nullptr) {
		m_create_button->on_click.connect([this] { create_project(); });
	}
	if (auto *browse = get<Button>("launcher-browse")) {
		browse->on_click.connect([this] { browse_location(); });
	}
	if (auto *cancel = get<Button>("launcher-cancel")) {
		cancel->on_click.connect([this] { show_page(Page::Home); });
	}
	if (auto *back = get<Clickable>("launcher-new-back")) {
		back->on_click.connect([this] { show_page(Page::Home); });
	}
}

void ProjectLauncher::wire_menu(PopupMenu *menu) {
	if (menu == nullptr) {
		return;
	}
	menu->set_action("open", [this] {
		if (m_context_project) {
			open_project(*m_context_project);
		}
	});
	menu->set_action("rename", [this] {
		if (m_context_project) {
			begin_action(ActionMode::Rename, *m_context_project);
		}
	});
	menu->set_action("delete", [this] {
		if (m_context_project) {
			begin_action(ActionMode::Delete, *m_context_project);
		}
	});
	menu->set_action("forget", [this] {
		if (m_context_project) {
			begin_action(ActionMode::Forget, *m_context_project);
		}
	});
}

void ProjectLauncher::show_page(Page page) {
	m_page = page;
	if (m_home_page != nullptr) {
		m_home_page->set_class("hidden", page != Page::Home);
	}
	if (m_new_page != nullptr) {
		m_new_page->set_class("hidden", page != Page::New);
	}
	if (page == Page::Home) {
		refresh_list();
	}
	if (page == Page::New && m_name_input != nullptr) {
		m_name_input->set_text(m_restored_name.value_or(default_project_name()));
		m_restored_name.reset();
		if (m_error_label != nullptr) {
			m_error_label->set_class("hidden", true);
		}
		update_new_form();
		m_name_input->request_focus();
	}
}

void ProjectLauncher::refresh_list() {
	if (m_recent_host == nullptr || m_recent_scroll == nullptr || m_empty_state == nullptr) {
		return;
	}

	m_recent_host->clear_children();
	const std::vector<ProjectInfo> projects = m_project_manager.list();
	m_empty_state->set_class("hidden", !projects.empty());
	m_recent_scroll->set_class("hidden", projects.empty());

	for (const ProjectInfo &project : projects) {
		auto *row =
			dynamic_cast<Clickable *>(create("LauncherRecent", *m_recent_host,
											 { .attributes = { { "name", project.name },
															   { "path", project.native_directory },
															   { "when", relative_time(project.last_activity()) }, }, }));
		if (row == nullptr) {
			continue;
		}
		row->on_click.connect([this, project] { open_project(project); });
		row->on_context_menu.connect([this, project](Vec2 pos) {
			m_context_project = project;
			PopupMenu *menu = ProjectManager::is_external(project) ? m_external_menu : m_context_menu;
			if (menu != nullptr) {
				menu->open_at(pos);
			}
		});
	}
}

void ProjectLauncher::browse_for_project() {
	const Option<std::string> folder = FileDialog::pick_folder(m_project_manager.get_native_root());
	if (!folder) {
		return;
	}
	if (Option<ProjectInfo> project = m_project_manager.add_existing(*folder)) {
		if (m_open_error != nullptr) {
			m_open_error->set_class("hidden", true);
		}
		open_project(*project);
		return;
	}
	if (m_open_error != nullptr) {
		m_open_error->set_text(std::format("No Aquila project in {} (it has no project.aqproject).", *folder));
		m_open_error->set_class("hidden", false);
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
	m_action_confirm->set_class("launcher-danger", mode == ActionMode::Delete);
	if (renaming) {
		m_action_confirm->set_text("Rename");
		m_action_label->set_text("Rename");
		m_action_input->set_text(project.name);
		m_action_input->request_focus();
	} else if (mode == ActionMode::Delete) {
		m_action_confirm->set_text("Delete");
		m_action_label->set_text(std::format("Delete '{}' and all of its files? This cannot be undone.", project.name));
	} else {
		m_action_confirm->set_text("Remove");
		m_action_label->set_text(
			std::format("Remove '{}' from the list? Its files stay in {}.", project.name, project.native_directory));
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
	} else if (m_action_mode == ActionMode::Forget) {
		if (!m_project_manager.forget(project)) {
			m_action_label->set_text("Could not update the project list");
			return;
		}
	}

	m_context_project.reset();
	end_action();
	refresh_list();
}

void ProjectLauncher::browse_location() {
	if (m_location_input == nullptr) {
		return;
	}
	const std::string &current = m_location_input->get_text();
	if (Option<std::string> folder =
			FileDialog::pick_folder(current.empty() ? m_project_manager.get_native_root() : current)) {
		m_location_input->set_text(*folder);
		update_new_form();
	}
}

std::string ProjectLauncher::default_project_name() const {
	const std::string location = m_location_input != nullptr ? m_location_input->get_text() : std::string{};
	std::string name = "New Project";
	for (int i = 2; i < 100 && !m_project_manager.validate_name(name, location).empty(); ++i) {
		name = std::format("New Project {}", i);
	}
	return name;
}

bool ProjectLauncher::update_new_form() {
	if (m_name_input == nullptr || m_name_error == nullptr || m_location_input == nullptr ||
		m_location_error == nullptr || m_preview == nullptr || m_create_button == nullptr) {
		return false;
	}
	const std::string &name = m_name_input->get_text();
	const std::string &location = m_location_input->get_text();
	const std::string location_error = ProjectManager::validate_location(location);
	std::string name_error;
	if (location_error.empty()) {
		name_error = m_project_manager.validate_name(name, location);
	} else if (Aquila::Foundation::trim(name).empty()) {
		name_error = "Enter a project name.";
	}

	m_name_error->set_text(name_error);
	m_name_error->set_class("hidden", name_error.empty());
	m_name_input->set_class("launcher-input-invalid", !name_error.empty());
	m_location_error->set_text(location_error);
	m_location_error->set_class("hidden", location_error.empty());
	m_location_input->set_class("launcher-input-invalid", !location_error.empty());

	const std::string folder = location_error.empty() ? m_project_manager.target_folder(name, location) : std::string{};
	m_preview->set_text(folder.empty() ? (std::filesystem::path(location) / "...").string() : folder);

	const bool valid = name_error.empty() && location_error.empty();
	m_create_button->set_class("launcher-create-disabled", !valid);
	return valid;
}

void ProjectLauncher::open_project(ProjectInfo project) {
	m_project_manager.touch(project);
	if (on_project_ready) {
		on_project_ready(project);
	}
}

void ProjectLauncher::create_project() {
	if (m_name_input == nullptr || m_location_input == nullptr || m_error_label == nullptr || !update_new_form()) {
		return;
	}

	const std::string name(Aquila::Foundation::trim(m_name_input->get_text()));
	if (Option<ProjectInfo> created =
			m_project_manager.create(name, ProjectTemplate::Empty, m_location_input->get_text())) {
		open_project(*created);
	} else {
		m_error_label->set_text("Could not create the project. Check the log for details.");
		m_error_label->set_class("hidden", false);
	}
}

} // namespace Editor
