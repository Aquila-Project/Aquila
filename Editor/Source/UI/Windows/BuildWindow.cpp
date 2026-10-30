#include "UI/Windows/BuildWindow.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Platform/Events/Event.h"
#include "Aquila/Platform/Events/InputEvent.h"
#include "Aquila/Platform/Events/KeyChord.h"
#include "Aquila/Platform/Filesystem/Filesystem.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Platform/Process.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/ReflectedPropertyGrid.h"
#include "Aquila/UI/Widgets/ScrollView.h"
#include "Aquila/UI/Widgets/SelectableTextView.h"
#include "Core/EditorContext.h"
#include "Core/FileDialog.h"
#include "Core/ProjectSettingsFields.h"
#include "UI/Components/BuildTemplateInfo.h"

namespace Editor {

using namespace Aquila::UI::Core;
namespace Player = Aquila::Player;
using Aquila::Platform::Filesystem::VirtualFileSystem;
using Aquila::Reflection::TypeInfo;
namespace Events = Aquila::Platform::Events;
namespace fs = std::filesystem;

namespace {

std::string describe(const Player::BuildStep &step) {
	switch (step.kind) {
	case Player::BuildStepKind::MakeDirectory:
		return std::format("make directory {}", step.target);
	case Player::BuildStepKind::CopyDirectory:
		return std::format("copy {} -> {}", step.source, step.target);
	case Player::BuildStepKind::Rename:
		return std::format("rename {} -> {}", step.source, step.target);
	case Player::BuildStepKind::WriteFile:
		return std::format("write {} ({} bytes)", step.target, step.content.size());
	case Player::BuildStepKind::CopyFile:
		return std::format("copy {} -> {}", step.source, step.target);
	}
	return "unknown step";
}

std::string template_dir() {
	return std::string(Player::player_template_dir(Aquila::Platform::Filesystem::path_executable_dir()));
}
}

BuildWindow::BuildWindow(EditorContext &context)
	: WindowBehaviour(context.window_layout("build")), m_context(context) {}

BuildWindow::~BuildWindow() {
	m_context.on_settings_changed.disconnect(m_settings_connection);
	if (m_worker.joinable()) {
		m_worker.join();
	}
}

void BuildWindow::define_behaviour() {
	if (auto *project = get<Label>("build-project")) {
		project->set_text(m_context.project().name);
	}
	m_status = get<Label>("build-status");
	m_log_scroll = get<ScrollView>("build-log");
	m_log = get<SelectableTextView>("build-log-text");

	m_grid = get<PropertyGrid>("build-grid");
	build_settings();
	m_settings_connection = m_context.on_settings_changed.connect([this] {
		if (m_fields && m_context.project().settings != m_settings) {
			m_settings = m_context.project().settings;
			m_fields->refresh();
		}
	});
	wire_buttons(root());

	log(LineKind::Info,
		std::format("Builds ship the scenes as they are saved in {}", m_context.project().native_directory));
	log(LineKind::Info, "Preview lists the steps of a build without writing anything.");
}

void BuildWindow::build_settings() {
	if (m_grid == nullptr) {
		return;
	}
	m_rebuild_settings = false;
	m_fields.reset();
	m_template = nullptr;
	m_grid->clear_children();

	const ProjectInfo &project = m_context.project();
	m_settings = project.settings;
	m_settings_type = std::make_unique<TypeInfo>("Build");
	add_build_output_fields(*m_settings_type);
	add_startup_scene_field(*m_settings_type, project);
	add_icon_field(*m_settings_type, project);
	add_window_size_fields(*m_settings_type);

	m_fields = std::make_unique<ReflectedPropertyGrid>(*m_grid, *m_settings_type, nullptr);
	m_fields->bind(&m_settings, [this] { m_context.set_project_settings(m_settings); });

	const auto add_browse_button = [this](std::string_view field, std::string tooltip, Delegate<void()> on_click) {
		View *widget = m_fields->get_widget(field);
		Unique<View> button =
			create_detached("BuildBrowseButton", { .attributes = { { "tooltip", std::move(tooltip) } } });
		if (widget == nullptr || button == nullptr) {
			return;
		}
		if (auto *added = dynamic_cast<Button *>(m_grid->add_action(*widget, std::move(button)))) {
			added->on_click.connect(std::move(on_click));
		}
	};
	add_browse_button("Output Folder", "Pick the output folder", [this] {
		const std::string current = m_settings.build_output.empty() ? default_build_output() : m_settings.build_output;
		if (Option<std::string> folder = FileDialog::pick_folder(current)) {
			m_settings.build_output = *folder;
			m_context.set_project_settings(m_settings);
			m_fields->refresh();
		}
	});
	add_browse_button("Icon", "Pick a PNG anywhere on disk, files outside the project are copied into Content",
					  [this] { pick_icon(); });

	if (Unique<View> info = create_detached("BuildTemplateInfo")) {
		if (View *row = m_grid->add_row("Player Template", std::move(info))) {
			m_template = find_behaviour<BuildTemplateInfo>(*row);
		}
	}
	refresh_template();
}

void BuildWindow::pick_icon() {
	const ProjectInfo &project = m_context.project();
	const std::string content(Aquila::SharedConstants::PROJECT_CONTENT_DIR);
	const std::array filters = { FileDialog::Filter{ .name = "PNG image", .extensions = "png" } };
	const Option<std::string> png = FileDialog::open_file(filters, m_context.to_native(content));
	if (!png) {
		return;
	}
	Option<std::string> inside = m_context.to_project_path(*png);
	if (!inside) {
		const std::string target = VirtualFileSystem::get()->unique_path(content, fs::path(*png).filename().string());
		std::error_code error;
		fs::copy_file(*png, m_context.to_native(target), error);
		if (error) {
			log(LineKind::Error, std::format("Couldn't copy {} into the project: {}", *png, error.message()));
			return;
		}
		log(LineKind::Info, std::format("Copied the icon to {}", target));
		inside = target;
	}
	const std::string mount = Aquila::SharedConstants::PROJECT_MOUNT + "/";
	m_settings.icon = inside->starts_with(mount) ? inside->substr(mount.size()) : *inside;
	m_context.set_project_settings(m_settings);
	m_rebuild_settings = true;
	Aquila::Foundation::FrameScheduler::get()->request_frame(root().get_canvas()->get_frame_target());
}

void BuildWindow::wire_buttons(View &root) {
	const std::array<std::pair<std::string_view, Delegate<void()>>, 3> actions{ {
		{ "build-preview", [this] { preview(); } },
		{ "build-and-run", [this] { start_build(true); } },
		{ "build-start", [this] { start_build(false); } },
	} };
	for (Usize i = 0; i < actions.size(); ++i) {
		m_buttons[i] = root.find_by_id<Button>(std::string(actions[i].first));
		if (m_buttons[i] != nullptr) {
			m_buttons[i]->on_click.connect(actions[i].second);
		}
	}

	const std::string chord = m_context.shortcuts.get_label("project.build");
	if (Button *build = m_buttons[2]; build != nullptr && !chord.empty()) {
		build->set_tooltip(std::format("Build the project into the output folder ({})", chord));
	}

	if (auto *close = root.find_by_id<Button>("build-close")) {
		close->on_click.connect([this] { this->close(); });
	}
}

void BuildWindow::update(F32) {
	if (m_rebuild_settings) {
		build_settings();
	}
	std::vector<Message> inbox;
	{
		std::scoped_lock lock(m_mutex);
		inbox.swap(m_inbox);
	}
	for (Message &message : inbox) {
		if (message.finished) {
			finish(message.result);
		} else {
			log(message.kind, std::move(message.text));
		}
	}
}

bool BuildWindow::intercept_event(Events::Event &event) {
	if (!is_build_chord(event)) {
		return false;
	}
	start_build(false);
	return true;
}

bool BuildWindow::is_build_chord(Events::Event &event) const {
	auto *pressed = dynamic_cast<Events::KeyPressedEvent *>(&event);
	if (pressed == nullptr || pressed->is_repeat()) {
		return false;
	}
	const Events::KeyChord chord = Events::KeyChord::from(*pressed);
	for (const ShortcutBinding &binding : m_context.shortcuts.get_bindings("project.build")) {
		if (binding.scope.empty() && binding.chord == chord) {
			return true;
		}
	}
	return false;
}

Player::BuildRequest BuildWindow::make_request() const {
	const ProjectInfo &project = m_context.project();
	const ProjectSettings &settings = project.settings;
	const bool first_scene = settings.startup_scene.empty() && !project.scenes.empty();
	return {
		.project_name = project.name,
		.project_dir = project.native_directory,
		.startup_scene = first_scene ? project.scenes.front() : settings.startup_scene,
		.template_dir = template_dir(),
		.output_dir = settings.build_output.empty() ? default_build_output() : settings.build_output,
		.window_width = settings.window_width,
		.window_height = settings.window_height,
		.icon_path =
			settings.icon.empty() ? std::string() : (fs::path(project.native_directory) / settings.icon).string(),
		.window_title = settings.window_title,
		.maximized = settings.start_maximized,
		.tone_mapper = settings.tone_mapper,
		.shadows = settings.shadows,
	};
}

Option<Player::BuildPlan> BuildWindow::plan(bool show_steps) {
	clear_log();
	refresh_template();

	Player::BuildPlan plan = Player::plan_build(make_request());
	if (!plan.ok()) {
		for (const std::string &error : plan.errors) {
			log(LineKind::Error, error);
		}
		set_status(StatusKind::Error, std::format("The plan has {} error(s)", plan.errors.size()));
		return std::nullopt;
	}
	if (show_steps) {
		log(LineKind::Info, std::format("Output: {}", plan.layout.root));
		for (Usize i = 0; i < plan.steps.size(); ++i) {
			log(LineKind::Step, std::format("{:>2}. {}", i + 1, describe(plan.steps[i])));
		}
	}
	set_status(StatusKind::Idle, std::format("{} steps", plan.steps.size()));
	return plan;
}

void BuildWindow::preview() {
	if (!m_busy) {
		plan(true);
	}
}

void BuildWindow::start_build(bool run_after) {
	if (m_busy) {
		return;
	}
	Option<Player::BuildPlan> planned = plan(false);
	if (!planned) {
		return;
	}
	if (m_worker.joinable()) {
		m_worker.join();
	}

	m_run_after = run_after;
	set_busy(true);
	set_status(StatusKind::Busy, "Building...");
	log(LineKind::Info, std::format("Building into {}", planned->layout.root));
	m_worker = std::thread([this, build_plan = std::move(*planned)] {
		Player::BuildResult result = Player::run_build(build_plan, [this](const Player::BuildProgress &progress) {
			post({ .kind = LineKind::Step,
				   .text = std::format("[{}/{}] {}", progress.step, progress.step_count, progress.message) });
		});
		post({ .kind = LineKind::Info, .text = {}, .finished = true, .result = std::move(result) });
	});
}

void BuildWindow::finish(const Player::BuildResult &result) {
	set_busy(false);
	for (const std::string &error : result.errors) {
		log(LineKind::Error, error);
	}

	if (!result.success) {
		set_status(StatusKind::Error, "Build failed");
		return;
	}

	log(LineKind::Success, std::format("Built {}", result.executable));
	set_status(StatusKind::Success, "Build succeeded");

	if (m_run_after) {
		if (Aquila::Platform::launch_detached(result.executable)) {
			log(LineKind::Info, "Launched the game");
		} else {
			log(LineKind::Error, std::format("Could not launch {}", result.executable));
		}
	}
}

void BuildWindow::refresh_template() {
	if (m_template == nullptr) {
		return;
	}
	const std::string directory = template_dir();
	m_template->show(fs::exists(fs::path(directory) / AQUILA_PLAYER_EXECUTABLE_NAME), directory);
}

void BuildWindow::set_busy(bool busy) {
	m_busy = busy;
	for (Button *button : m_buttons) {
		if (button != nullptr) {
			button->set_enabled(!busy);
		}
	}
}

void BuildWindow::set_status(StatusKind kind, std::string text) {
	if (m_status == nullptr) {
		return;
	}
	m_status->set_text(std::move(text));
	m_status->set_class("build-status-busy", kind == StatusKind::Busy);
	m_status->set_class("build-status-error", kind == StatusKind::Error);
	m_status->set_class("build-status-success", kind == StatusKind::Success);
}

void BuildWindow::clear_log() {
	if (m_log != nullptr) {
		m_log->clear();
	}
}

void BuildWindow::log(LineKind kind, std::string text) {
	if (m_log == nullptr) {
		return;
	}
	constexpr std::array<std::string_view, 5> classes{ "build-log-info", "", "build-log-warning", "build-log-error",
													   "build-log-success" };
	m_log->add_line(std::move(text), std::string(classes[static_cast<Usize>(kind)]));
	if (m_log_scroll != nullptr) {
		m_log_scroll->scroll_to_bottom();
	}
}

void BuildWindow::post(Message message) {
	{
		std::scoped_lock lock(m_mutex);
		m_inbox.push_back(std::move(message));
	}
	Aquila::Foundation::FrameScheduler::get()->request_frame();
}

}
