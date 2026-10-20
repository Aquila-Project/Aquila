#include "UI/Windows/BuildWindow.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Platform/Events/Event.h"
#include "Aquila/Platform/Events/InputEvent.h"
#include "Aquila/Platform/Events/KeyChord.h"
#include "Aquila/Platform/Events/WindowEvent.h"
#include "Aquila/Platform/Filesystem/Filesystem.h"
#include "Aquila/Platform/Process.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/DragInt.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/ScrollView.h"
#include "Aquila/UI/Widgets/SelectableTextView.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Core/EditorContext.h"
#include "Core/FileDialog.h"

namespace Editor {

using namespace Aquila::UI::Core;
namespace Player = Aquila::Player;
namespace Events = Aquila::Platform::Events;
namespace fs = std::filesystem;

namespace {

constexpr int k_min_window_size = 320;
constexpr int k_max_window_size = 7680;
constexpr std::string_view k_default_icon = "default";

constexpr Vec4 k_info_color{ 0.62F, 0.62F, 0.62F, 1.F };
constexpr Vec4 k_warning_color{ 0.83F, 0.67F, 0.29F, 1.F };
constexpr Vec4 k_error_color{ 0.87F, 0.45F, 0.45F, 1.F };
constexpr Vec4 k_success_color{ 0.29F, 0.72F, 0.44F, 1.F };

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

template <typename T, typename... Args> T *add_field(View &form, std::string label, bool top, Args &&...args) {
	auto *row = form.add_child<View>();
	row->add_class("build-row");
	row->set_class("build-row-top", top);
	auto *name = row->add_child<Label>(std::move(label));
	name->add_class("build-row-label");
	auto *field = row->add_child<T>(std::forward<Args>(args)...);
	field->add_class("build-field");
	return field;
}

std::string template_dir() {
	return std::string(Player::player_template_dir(Aquila::Platform::Filesystem::path_executable_dir()));
}

void add_browse_button(View &field, Aquila::GFX::GfxTexture *icon, std::string tooltip, Delegate<void()> on_click) {
	View *row = field.get_parent();
	if (row == nullptr) {
		return;
	}
	auto *button = row->add_child<Button>();
	button->set_icon(icon);
	button->add_class("build-btn");
	button->add_class("build-browse");
	button->set_tooltip(std::move(tooltip));
	button->on_click.connect(std::move(on_click));
}

std::vector<fs::path> project_pngs(const fs::path &content) {
	std::vector<fs::path> files;
	std::error_code error;
	for (fs::recursive_directory_iterator it(content, error), end; !error && it != end; it.increment(error)) {
		std::string extension = it->path().extension().string();
		std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return std::tolower(c); });
		if (it->is_regular_file(error) && extension == ".png") {
			files.push_back(it->path());
		}
	}
	std::ranges::sort(files);
	return files;
}

}

BuildWindow::BuildWindow(EditorContext &context) : m_context(context) {}

BuildWindow::~BuildWindow() {
	if (m_worker.joinable()) {
		m_worker.join();
	}
}

void BuildWindow::build(Uint32 width, Uint32 height, const std::vector<std::string> &style_paths) {
	m_canvas = std::make_unique<Canvas>(width, height);
	Aquila::UI::StyleParser::load_files(style_paths, m_canvas->get_style_sheet());

	View *root = m_canvas->get_root();
	if (auto layout = m_context.load_layout("build.aqlayout")) {
		root = root->add_child(std::move(layout));
	}

	if (auto *project = root->find_by_id<Label>("build-project")) {
		project->set_text(m_context.project().name);
	}
	m_status = root->find_by_id<Label>("build-status");
	m_log_scroll = root->find_by_id<ScrollView>("build-log");
	if (m_log_scroll != nullptr) {
		m_log = dynamic_cast<SelectableTextView *>(m_log_scroll->add_child(std::make_unique<SelectableTextView>()));
		m_log->add_class("build-log-text");
	}

	if (auto *settings = root->find_by_id("build-settings")) {
		build_settings(*settings);
	}
	wire_buttons(*root);

	refresh_template();
	log(LineKind::Info,
		std::format("Builds ship the scenes as they are saved in {}", m_context.project().native_directory));
	log(LineKind::Info, "Preview lists the steps of a build without writing anything.");

	m_canvas->reload_styles();
}

void BuildWindow::build_settings(View &content) {
	const ProjectInfo &project = m_context.project();

	auto *form = content.add_child<View>();
	form->add_class("build-form");

	m_output = add_field<TextInput>(*form, "Output", false, std::string("Folder the game is written to"));
	m_output->add_class("split-TextInput");
	const ProjectSettings &settings = project.settings;
	m_output->set_text(settings.build_output.empty() ? Aquila::Platform::Filesystem::path_executable_dir() + "/Builds"
													 : settings.build_output);
	add_browse_button(*m_output, m_context.icon("folder-open"), "Pick the output folder", [this] {
		if (Option<std::string> folder = FileDialog::pick_folder(m_output->get_text())) {
			m_output->set_text(*folder);
		}
	});

	m_scene = add_field<Dropdown>(*form, "Startup scene", false);
	m_scene->set_variant("props-dropdown");
	m_scene->set_chevron(m_context.icon("chevron-down"));
	m_scene->set_placeholder("No saved scenes");
	for (const std::string &scene : project.scenes) {
		m_scene->add_option(scene, scene);
	}
	if (std::ranges::find(project.scenes, settings.startup_scene) != project.scenes.end()) {
		m_scene->set_value(settings.startup_scene);
	} else if (!project.scenes.empty()) {
		m_scene->set_value(project.scenes.front());
	}

	auto *size = add_field<View>(*form, "Window size", false);
	size->add_class("build-size");
	m_width = size->add_child<DragInt>();
	auto *times = size->add_child<Label>(std::string("\u00d7"));
	times->add_class("build-size-times");
	m_height = size->add_child<DragInt>();
	for (DragInt *field : { m_width, m_height }) {
		field->add_class("split-DragInt");
		field->add_class("build-size-field");
		field->set_int_range(k_min_window_size, k_max_window_size);
	}
	m_width->set_int_value(static_cast<int>(settings.window_width));
	m_height->set_int_value(static_cast<int>(settings.window_height));

	m_icon = add_field<Dropdown>(*form, "Icon", false);
	m_icon->set_variant("props-dropdown");
	m_icon->set_chevron(m_context.icon("chevron-down"));
	m_icon->set_icon(m_context.icon("image"));
	m_icon->add_option(std::string(k_default_icon), "Aquila logo");
	const fs::path content_dir = fs::path(project.native_directory) / Aquila::SharedConstants::PROJECT_CONTENT_FOLDER;
	for (const fs::path &png : project_pngs(content_dir)) {
		m_icon->add_option(png.string(), png.lexically_relative(content_dir).generic_string());
	}
	m_icon->set_value(std::string(k_default_icon));
	if (!settings.icon.empty()) {
		const fs::path icon = fs::path(project.native_directory) / settings.icon;
		if (fs::is_regular_file(icon)) {
			m_icon->add_option(icon.string(), settings.icon);
			m_icon->set_value(icon.string());
		}
	}
	m_icon->set_tooltip("PNGs in the project's Content folder. The window and taskbar use it.");
	add_browse_button(*m_icon, m_context.icon("folder-open"), "Pick a PNG anywhere on disk", [this, content_dir] {
		const std::array filters = { FileDialog::Filter{ .name = "PNG image", .extensions = "png" } };
		const Option<std::string> png = FileDialog::open_file(filters, content_dir.string());
		if (!png) {
			return;
		}
		const fs::path relative = fs::path(*png).lexically_relative(content_dir);
		const bool inside = !relative.empty() && *relative.begin() != "..";
		m_icon->add_option(*png, inside ? relative.generic_string() : *png);
		m_icon->set_value(*png);
	});

	auto *template_info = add_field<View>(*form, "Player template", true);
	template_info->add_class("build-template");
	m_template_state = template_info->add_child<Label>(std::string());
	m_template_state->add_class("build-template-state");
	m_template_path = template_info->add_child<SelectableTextView>();
	m_template_path->add_class("build-template-path");
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
		close->on_click.connect([this] {
			if (on_request_close) {
				on_request_close();
			}
		});
	}
}

void BuildWindow::update(F32 delta_time) {
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

	m_canvas->update(delta_time);
	m_canvas->compute();
}

void BuildWindow::render(Aquila::Graphics::Renderer2D &batcher, Aquila::GFX::GfxCommandList &cmd) {
	m_canvas->submit_to_quad_batcher(batcher, cmd);
}

void BuildWindow::set_frame_target(const void *target) {
	m_canvas->set_frame_target(target);
}

Aquila::Platform::CursorType BuildWindow::get_cursor() const {
	const View *hovered = m_canvas->get_hovered_view();
	return hovered != nullptr ? hovered->get_cursor() : Aquila::Platform::CursorType::Arrow;
}

void BuildWindow::on_event(Events::Event &event) {
	Events::EventDispatcher dispatcher(event);
	dispatcher.dispatch<Events::WindowResizeEvent>([this](Events::WindowResizeEvent &e) {
		if (e.get_width() > 0 && e.get_height() > 0) {
			m_canvas->resize(e.get_width(), e.get_height());
		}
		return false;
	});

	if (is_build_chord(event)) {
		start_build(false);
		return;
	}
	m_canvas->on_event(event);
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

void BuildWindow::remember_choices() {
	const ProjectInfo &project = m_context.project();
	ProjectSettings settings = project.settings;
	if (m_scene != nullptr) {
		settings.startup_scene = m_scene->get_value();
	}
	if (m_output != nullptr) {
		const std::string fallback = Aquila::Platform::Filesystem::path_executable_dir() + "/Builds";
		settings.build_output = m_output->get_text() == fallback ? std::string() : m_output->get_text();
	}
	if (m_width != nullptr && m_height != nullptr) {
		settings.window_width = static_cast<Uint32>(m_width->get_int_value());
		settings.window_height = static_cast<Uint32>(m_height->get_int_value());
	}
	if (m_icon != nullptr) {
		const std::string value = m_icon->get_value();
		const fs::path relative = fs::path(value).lexically_relative(project.native_directory);
		const bool inside = value != k_default_icon && !relative.empty() && *relative.begin() != "..";
		if (value == k_default_icon) {
			settings.icon.clear();
		} else if (inside) {
			settings.icon = relative.generic_string();
		}
	}
	m_context.set_project_settings(std::move(settings));
}

Player::BuildRequest BuildWindow::make_request() const {
	const ProjectInfo &project = m_context.project();
	return {
		.project_name = project.name,
		.project_dir = project.native_directory,
		.startup_scene = m_scene != nullptr ? m_scene->get_value() : std::string(),
		.template_dir = template_dir(),
		.output_dir = m_output != nullptr ? m_output->get_text() : std::string(),
		.window_width = static_cast<Uint32>(m_width != nullptr ? m_width->get_int_value() : 0),
		.window_height = static_cast<Uint32>(m_height != nullptr ? m_height->get_int_value() : 0),
		.icon_path = m_icon != nullptr && m_icon->get_value() != k_default_icon ? m_icon->get_value() : std::string(),
		.window_title = project.settings.window_title,
		.maximized = project.settings.start_maximized,
		.tone_mapper = project.settings.tone_mapper,
		.shadows = project.settings.shadows,
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
	if (plan.steps.empty()) {
		log(LineKind::Warning, "plan_build returned no steps and no errors.");
		set_status(StatusKind::Error, "Nothing to build");
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
	remember_choices();
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
		if (result.errors.empty()) {
			log(LineKind::Warning, "run_build failed without saying why.");
		}
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
	if (m_template_state == nullptr || m_template_path == nullptr) {
		return;
	}
	const std::string directory = template_dir();
	const bool stub = directory.empty();
	const bool found = !stub && fs::exists(fs::path(directory) / AQUILA_PLAYER_EXECUTABLE_NAME);

	if (stub) {
		m_template_state->set_text("player_template_dir() is still a stub");
	} else if (found) {
		m_template_state->set_text("Found");
	} else {
		m_template_state->set_text("Missing: build the PlayerTemplate target");
	}
	m_template_state->set_class("build-template-found", found);
	m_template_state->set_class("build-template-missing", !found);

	m_template_path->clear();
	m_template_path->set_class("hidden", stub);
	if (!stub) {
		m_template_path->add_line(directory);
	}
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
	constexpr std::array<Vec4, 5> colors{ k_info_color, Vec4(0.F), k_warning_color, k_error_color, k_success_color };
	m_log->add_line(std::move(text), colors[static_cast<Usize>(kind)]);
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
