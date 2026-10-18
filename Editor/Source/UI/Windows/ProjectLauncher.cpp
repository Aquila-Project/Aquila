#include "UI/Windows/ProjectLauncher.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Platform/Events/Event.h"
#include "Aquila/Platform/Events/WindowEvent.h"
#include "Aquila/Platform/Input.h"
#include "Aquila/Platform/Process.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/TextureCache.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Aquila/UI/Widgets/ScrollView.h"
#include "Aquila/UI/Widgets/TextInput.h"

namespace Editor {

using namespace Aquila;
using namespace Aquila::UI::Core;

namespace {

constexpr int K_CARDS_PER_ROW = 3;
constexpr int K_THUMB_PALETTE = 6;
constexpr const char *K_WIKI_URL = "https://github.com/Aquila-Project/Aquila/wiki";

class ClickableView : public View {
  public:
	ClickableView() { set_input_leaf(true); }

	Signal<void()> on_click;

	void on_mouse_release(Platform::MouseButton btn, Vec2 pos) override {
		const bool was_pressed = m_is_pressed;
		View::on_mouse_release(btn, pos);
		if (btn == Platform::MouseButton::Left && m_is_hovered && was_pressed) {
			on_click();
		}
	}
};

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

Label *add_label(View *parent, const std::string &text, const char *css_class) {
	auto *label = parent->add_child<Label>(text);
	label->add_class(css_class);
	return label;
}

View *add_view(View *parent, const char *css_class) {
	auto *view = parent->add_child<View>();
	view->add_class(css_class);
	return view;
}

ClickableView *add_card(View *row, const std::string &title, const std::string &meta, const std::string &letter,
						const std::string &thumb_class) {
	auto *card = row->add_child<ClickableView>();
	card->add_class("launcher-card");

	auto *thumb = add_view(card, "launcher-thumb");
	thumb->add_class(thumb_class);
	add_label(thumb, letter, "launcher-thumb-letter");

	auto *body = add_view(card, "launcher-card-body");
	add_label(body, title, "launcher-card-name");
	add_label(body, meta, "launcher-card-meta");
	return card;
}

} // namespace

ProjectLauncher::ProjectLauncher() = default;
ProjectLauncher::~ProjectLauncher() = default;

void ProjectLauncher::build(ProjectManager *project_manager, TextureCache *texture_cache, Uint32 width, Uint32 height,
							const std::vector<std::string> &style_paths) {
	m_project_manager = project_manager;
	m_texture_cache = texture_cache;
	if (m_texture_cache != nullptr) {
		m_logo = m_texture_cache->load("Engine/aquila-logo.png");
	}

	m_canvas = std::make_unique<Canvas>(width, height);
	UI::StyleParser::load_files(style_paths, m_canvas->get_style_sheet());

	auto *root = m_canvas->get_root();
	root->set_id("launcher-root");
	root->add_class("launcher-root");

	build_header(root);

	auto *main = add_view(root, "launcher-main");
	build_rail(main);

	auto *content = add_view(main, "launcher-content");
	build_recent_page(content);
	build_new_page(content);
	build_learn_page(content);

	m_context_menu = root->add_child<PopupMenu>();
	m_context_menu->add_item("Open", [this] {
		if (m_context_project) {
			open_project(*m_context_project);
		}
	});
	m_context_menu->add_item("Rename", [this] {
		if (m_context_project) {
			begin_action(ActionMode::Rename, *m_context_project);
		}
	});
	m_context_menu->add_item("Delete", [this] {
		if (m_context_project) {
			begin_action(ActionMode::Delete, *m_context_project);
		}
	});

	select_template(ProjectTemplate::Empty);
	refresh_list();
	show_page(Page::Recent);
	m_canvas->reload_styles();
}

void ProjectLauncher::build_header(View *root) {
	auto *header = add_view(root, "launcher-header");

	auto *logo = header->add_child<Image>(m_logo);
	logo->add_class("launcher-logo");

	auto *brand = add_view(header, "launcher-brand");
	add_label(brand, "Aquila", "launcher-title");
	add_label(brand, "Real-time rendering sandbox", "launcher-tagline");

	add_view(header, "launcher-header-fill");
	add_label(header, std::format("v{}.{}.{}", AQUILA_VERSION_MAJOR, AQUILA_VERSION_MINOR, AQUILA_VERSION_PATCH),
			  "launcher-version");
}

void ProjectLauncher::build_rail(View *main) {
	auto *rail = add_view(main, "launcher-rail");

	struct NavItem {
		Page page;
		const char *text;
		const char *icon;
	};
	const NavItem items[] = {
		{ Page::Recent, "Recent projects", "Engine/UI/Icons/folder-open.svg" },
		{ Page::New, "New project", "Engine/UI/Icons/plus.svg" },
		{ Page::Learn, "Learn", "Engine/UI/Icons/book-open-text.svg" },
	};
	for (const NavItem &item : items) {
		auto *button = rail->add_child<Button>(std::string(item.text));
		button->add_class("launcher-nav");
		if (m_texture_cache != nullptr) {
			button->set_icon(m_texture_cache->load(item.icon));
		}
		const Page page = item.page;
		button->on_click.connect([this, page] { show_page(page); });
		m_nav_buttons[static_cast<int>(page)] = button;
	}
}

void ProjectLauncher::build_recent_page(View *content) {
	auto *page = add_view(content, "launcher-page");
	m_pages[static_cast<int>(Page::Recent)] = page;

	auto *title_row = add_view(page, "launcher-title-row");
	add_label(title_row, "Recent projects", "launcher-page-title");
	m_recent_count = add_label(title_row, "", "launcher-page-count");

	auto *scroll = page->add_child<ScrollView>();
	scroll->add_class("launcher-grid-scroll");
	m_grid_scroll = scroll;
	m_grid_host = add_view(scroll, "launcher-grid");

	m_empty_state = add_view(page, "launcher-empty");
	auto *empty_logo = m_empty_state->add_child<Image>(m_logo);
	empty_logo->add_class("launcher-empty-logo");
	add_label(m_empty_state, "No projects yet", "launcher-empty-title");
	add_label(m_empty_state, "Create a project to start building a scene, a shader or a rendering pipeline.",
			  "launcher-empty-hint");
	auto *empty_button = m_empty_state->add_child<Button>(std::string("Create your first project"));
	empty_button->add_class("launcher-primary");
	empty_button->on_click.connect([this] { show_page(Page::New); });

	m_action_bar = add_view(page, "launcher-action-bar");
	m_action_label = add_label(m_action_bar, "", "launcher-action-label");
	m_action_input = m_action_bar->add_child<TextInput>(std::string("Project name"));
	m_action_input->add_class("launcher-action-input");
	m_action_input->on_submit.connect([this](const std::string &) { confirm_action(); });
	m_action_confirm = m_action_bar->add_child<Button>(std::string("Confirm"));
	m_action_confirm->add_class("launcher-primary");
	m_action_confirm->on_click.connect([this] { confirm_action(); });
	auto *cancel = m_action_bar->add_child<Button>(std::string("Cancel"));
	cancel->add_class("launcher-secondary");
	cancel->on_click.connect([this] { end_action(); });
	end_action();
}

void ProjectLauncher::build_new_page(View *content) {
	auto *page = add_view(content, "launcher-page");
	m_pages[static_cast<int>(Page::New)] = page;

	add_label(page, "New project", "launcher-page-title");

	add_label(page, "Template", "launcher-section");
	auto *templates = add_view(page, "launcher-template-row");
	const ProjectTemplate options[] = { ProjectTemplate::Empty, ProjectTemplate::Sandbox };
	for (const ProjectTemplate option : options) {
		auto *card = templates->add_child<ClickableView>();
		card->add_class("launcher-template");
		add_label(card, ProjectManager::template_name(option), "launcher-template-name");
		add_label(card, ProjectManager::template_description(option), "launcher-template-desc");
		card->on_click.connect([this, option] { select_template(option); });
		m_template_cards[static_cast<int>(option)] = card;
	}

	add_label(page, "Name", "launcher-section");
	m_name_input = page->add_child<TextInput>(std::string("My project"));
	m_name_input->add_class("launcher-name-input");
	m_name_input->on_changed.connect([this](const std::string &) { update_new_form(); });
	m_name_input->on_submit.connect([this](const std::string &) { create_project(); });

	m_location_label = add_label(page, "", "launcher-hint");
	m_error_label = add_label(page, "", "launcher-error");

	auto *actions = add_view(page, "launcher-form-actions");
	m_create_button = actions->add_child<Button>(std::string("Create project"));
	m_create_button->add_class("launcher-primary");
	m_create_button->on_click.connect([this] { create_project(); });
	auto *back = actions->add_child<Button>(std::string("Back"));
	back->add_class("launcher-secondary");
	back->on_click.connect([this] { show_page(Page::Recent); });
}

void ProjectLauncher::build_learn_page(View *content) {
	auto *page = add_view(content, "launcher-page");
	m_pages[static_cast<int>(Page::Learn)] = page;

	add_label(page, "Learn", "launcher-page-title");

	add_label(page, "Documentation", "launcher-section");
	auto *docs = page->add_child<ClickableView>();
	docs->add_class("launcher-link");
	add_label(docs, "Aquila wiki", "launcher-tip-title");
	add_label(docs, "Guides and reference for the engine and the editor.", "launcher-tip-text");
	add_label(docs, "github.com/Aquila-Project/Aquila/wiki", "launcher-link-url");
	docs->on_click.connect([] {
		if (!Platform::open_url(K_WIKI_URL)) {
			AQUILA_LOG_ERROR("ProjectLauncher: could not open {}", K_WIKI_URL);
		}
	});

	struct Tip {
		const char *title;
		const char *text;
	};
	const Tip tips[] = {
		{ .title = "Build a scene",
		  .text = "Add entities from the Hierarchy panel and edit their components in the Inspector." },
		{ .title = "Move around",
		  .text = "Hold the right mouse button in the viewport to fly. Use W, E and R for the transform tools." },
		{ .title = "Scale the interface", .text = "Ctrl plus, Ctrl minus and Ctrl zero change the interface scale." },
	};
	add_label(page, "Tips", "launcher-section");
	auto *list = add_view(page, "launcher-tips");
	for (const Tip &tip : tips) {
		auto *row = add_view(list, "launcher-tip");
		add_label(row, tip.title, "launcher-tip-title");
		add_label(row, tip.text, "launcher-tip-text");
	}
}

void ProjectLauncher::show_page(Page page) {
	m_page = page;
	for (int i = 0; i < 3; ++i) {
		const bool active = i == static_cast<int>(page);
		if (m_pages[i] != nullptr) {
			m_pages[i]->set_class("hidden", !active);
		}
		if (m_nav_buttons[i] != nullptr) {
			m_nav_buttons[i]->set_class("launcher-nav-active", active);
		}
	}
	if (page == Page::Recent) {
		refresh_list();
	}
	if (page == Page::New) {
		update_new_form();
		m_name_input->request_focus();
	}
}

void ProjectLauncher::refresh_list() {
	if (m_grid_host == nullptr || m_project_manager == nullptr) {
		return;
	}

	while (!m_grid_host->get_children().empty()) {
		m_grid_host->remove_child(m_grid_host->get_children().front().get());
	}

	const std::vector<ProjectInfo> projects = m_project_manager->list();
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
	auto next_row = [&] {
		if (row == nullptr || in_row == K_CARDS_PER_ROW) {
			row = add_view(m_grid_host, "launcher-row");
			in_row = 0;
		}
		++in_row;
		return row;
	};

	auto *new_card = add_card(next_row(), "New project", "Start from a template", "+", "launcher-thumb-new");
	new_card->on_click.connect([this] { show_page(Page::New); });

	for (const ProjectInfo &project : projects) {
		const std::size_t scene_count = project.scenes.size();
		const std::string meta = std::format("Opened {}  ·  {} scene{}", relative_time(project.last_activity()),
											 scene_count, scene_count == 1 ? "" : "s");
		const std::size_t palette = std::hash<std::string>{}(project.name) % K_THUMB_PALETTE;
		auto *card = add_card(next_row(), project.name, meta, initial_of(project.name),
							  std::format("launcher-thumb-{}", palette));
		card->on_click.connect([this, project] { open_project(project); });
		card->on_context_menu.connect([this, project](Vec2 pos) {
			m_context_project = project;
			m_context_menu->open_at(pos);
		});
	}

	while (in_row < K_CARDS_PER_ROW) {
		add_view(row, "launcher-spacer");
		++in_row;
	}
}

void ProjectLauncher::begin_action(ActionMode mode, const ProjectInfo &project) {
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
	m_action_bar->set_class("hidden", true);
}

void ProjectLauncher::confirm_action() {
	if (!m_context_project || m_project_manager == nullptr) {
		end_action();
		return;
	}

	ProjectInfo project = *m_context_project;
	if (m_action_mode == ActionMode::Rename) {
		if (!m_project_manager->rename(project, m_action_input->get_text())) {
			m_action_label->set_text("Enter a name for the project");
			return;
		}
	} else if (m_action_mode == ActionMode::Delete) {
		if (!m_project_manager->remove(project)) {
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
	if (m_name_input == nullptr || m_project_manager == nullptr) {
		return;
	}
	const std::string &name = m_name_input->get_text();
	const std::string error = m_project_manager->validate_name(name);
	const bool has_name = !name.empty();

	const std::string safe = ProjectManager::sanitize(name);
	m_location_label->set_text(safe.empty() ? "" : std::format("Will be created in Projects/{}", safe));
	m_error_label->set_text(has_name ? error : "");
	m_error_label->set_class("hidden", !has_name || error.empty());
	m_create_button->set_class("launcher-primary-disabled", !error.empty());
}

void ProjectLauncher::open_project(ProjectInfo project) {
	if (m_project_manager != nullptr) {
		m_project_manager->touch(project);
	}
	if (on_project_ready) {
		on_project_ready(project);
	}
}

void ProjectLauncher::create_project() {
	if (m_name_input == nullptr || m_project_manager == nullptr) {
		return;
	}

	const std::string name = m_name_input->get_text();
	update_new_form();
	if (!m_project_manager->validate_name(name).empty()) {
		return;
	}

	if (Option<ProjectInfo> created = m_project_manager->create(name, m_template)) {
		m_name_input->set_text("");
		open_project(*created);
	} else {
		m_error_label->set_text("Could not create the project. Check the log for details.");
		m_error_label->set_class("hidden", false);
	}
}

void ProjectLauncher::update(F32 delta_time) {
	m_canvas->update(delta_time);
	m_canvas->compute();
}

void ProjectLauncher::render(Graphics::Renderer2D &batcher, GFX::GfxCommandList &cmd) {
	m_canvas->submit_to_quad_batcher(batcher, cmd);
}

void ProjectLauncher::set_frame_target(const void *target) {
	m_canvas->set_frame_target(target);
}

void ProjectLauncher::on_event(Platform::Events::Event &event) {
	Platform::Events::EventDispatcher dispatcher(event);
	dispatcher.dispatch<Platform::Events::WindowResizeEvent>([this](Platform::Events::WindowResizeEvent &e) {
		if (e.get_width() > 0 && e.get_height() > 0) {
			m_canvas->resize(e.get_width(), e.get_height());
		}
		return false;
	});

	m_canvas->on_event(event);
}

} // namespace Editor
