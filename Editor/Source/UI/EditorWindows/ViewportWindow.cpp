#include "UI/EditorWindows/ViewportWindow.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/Profiler.h"
#include "Aquila/Platform/Events/InputEvent.h"
#include "Aquila/Platform/Input.h"
#include "Aquila/Rendering/CameraController.h"
#include "Aquila/Rendering/RenderPipeline.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/UI/Widgets/DragFloat.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/Rendering/Systems/ObjectPickingSystem.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Checkbox.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/Popup.h"
#include "UI/EditorWindows/GameWindow.h"
#include "UI/Panels/ViewportPanel.h"
#include "UI/Viewport/TransformGizmo.h"
#include "UI/Viewport/ViewportOverlay.h"

namespace Editor {

using namespace Aquila;
using SceneManagement::Entity;
namespace Events = Aquila::Platform::Events;

namespace {

struct ShadingMode {
	Rendering::DebugView view;
	const char *label;
	const char *icon;
};

constexpr std::array<ShadingMode, 7> k_shading_modes = { {
	{ .view = Rendering::DebugView::Lit, .label = "Lit", .icon = "sun" },
	{ .view = Rendering::DebugView::Albedo, .label = "Albedo", .icon = "palette" },
	{ .view = Rendering::DebugView::Normals, .label = "Normals", .icon = "arrow-up-from-dot" },
	{ .view = Rendering::DebugView::Depth, .label = "Depth", .icon = "blend" },
	{ .view = Rendering::DebugView::Wireframe, .label = "Wireframe", .icon = "box" },
	{ .view = Rendering::DebugView::ShadowCascades, .label = "Shadow Cascades", .icon = "layers-3" },
	{ .view = Rendering::DebugView::LightComplexity, .label = "Light Complexity", .icon = "flame" },
} };

struct ToneMapperOption {
	Rendering::ToneMapper mapper;
	const char *label;
};

constexpr std::array<ToneMapperOption, 4> k_tone_mappers = { {
	{ .mapper = Rendering::ToneMapper::AgX, .label = "AgX" },
	{ .mapper = Rendering::ToneMapper::Neutral, .label = "Khronos Neutral" },
	{ .mapper = Rendering::ToneMapper::Aces, .label = "ACES" },
	{ .mapper = Rendering::ToneMapper::None, .label = "None" },
} };

struct ShowToggle {
	const char *label;
	bool Rendering::RenderSettings::*flag;
};

constexpr std::array<ShowToggle, 5> k_show_toggles = { {
	{ .label = "Grid", .flag = &Rendering::RenderSettings::show_grid },
	{ .label = "Sky", .flag = &Rendering::RenderSettings::show_sky },
	{ .label = "Shadows", .flag = &Rendering::RenderSettings::show_shadows },
	{ .label = "Selection Outline", .flag = &Rendering::RenderSettings::show_outline },
	{ .label = "Realtime", .flag = &Rendering::RenderSettings::realtime },
} };

} // namespace

ViewportWindow::ViewportWindow(EditorContext &context) : EditorWindow(context) {}

ViewportWindow::~ViewportWindow() {
	if (Rendering::CameraController *camera = context().camera()) {
		camera->set_navigation_blocked(true);
	}
}

void ViewportWindow::build(UI::Core::View &content) {
	if (auto layout = load_layout("viewport.aqlayout")) {
		content.add_child(std::move(layout));
	}

	m_panel = std::make_unique<ViewportPanel>(context().engine().get_render_output());
	m_panel->build(&content, overlay_root());
	m_panel->on_clicked_uv.connect([this](Vec2 uv) { request_pick(uv); });

	listen(context().on_render_output_changed,
		   [this] { m_panel->set_texture(&context().engine().get_render_output()); });

	if (UI::Core::View *root = content.find_by_id("viewport-root")) {
		m_overlay = root->add_child<ViewportOverlay>(context());
		m_gizmo = root->add_child<TransformGizmo>(context());
		m_gizmo->on_background_pressed.connect([this](Vec2 position) {
			const Rect rect = m_panel->get_content_rect();
			if (rect.size.x > 0.F && rect.size.y > 0.F) {
				request_pick((position - rect.position) / rect.size);
			}
		});
	}

	wire_tools(content);
	wire_shading(content);
	wire_show_menu(content);
	wire_camera_menu(content);
	listen(context().selection.on_scene_replaced, [this] {
		m_view_camera = {};
		m_scene_cameras.clear();
	});

	register_commands();
}

void ViewportWindow::register_commands() {
	EditorTools &tools = context().tools;
	add_command({ .id = "transform.translate", .title = "Move", .run = [&tools] {
					 tools.set_tool(TransformTool::Translate);
				 } });
	add_command(
		{ .id = "transform.rotate", .title = "Rotate", .run = [&tools] { tools.set_tool(TransformTool::Rotate); } });
	add_command(
		{ .id = "transform.scale", .title = "Scale", .run = [&tools] { tools.set_tool(TransformTool::Scale); } });
	add_command({ .id = "transform.toggle_space", .title = "Toggle world/local space", .run = [&tools] {
					 tools.toggle_space();
				 } });
	add_command({ .id = "transform.toggle_snapping", .title = "Toggle snapping", .run = [&tools] {
					 tools.toggle_snapping();
				 } });

	constexpr std::array<std::pair<std::string_view, std::string_view>, 3> axes{ {
		{ "transform.axis_x", "Constrain to X" },
		{ "transform.axis_y", "Constrain to Y" },
		{ "transform.axis_z", "Constrain to Z" },
	} };
	for (int axis = 0; axis < 3; ++axis) {
		const auto &[id, title] = axes[static_cast<Usize>(axis)];
		add_command({
			.id = std::string(id),
			.title = std::string(title),
			.run = [this, axis] { m_gizmo->constrain_to_axis(axis); },
			.enabled = [this] { return can_constrain(); },
		});
	}

	add_command({
		.id = "transform.confirm",
		.title = "Confirm transform",
		.run = [this] { m_gizmo->confirm(); },
		.enabled = [this] { return m_gizmo != nullptr && m_gizmo->is_modal(); },
	});
	add_command({
		.id = "transform.cancel",
		.title = "Cancel transform",
		.run = [this] { m_gizmo->cancel(); },
		.enabled = [this] { return m_gizmo != nullptr && m_gizmo->is_active(); },
	});
}

bool ViewportWindow::can_constrain() const {
	if (m_gizmo == nullptr || m_panel == nullptr) {
		return false;
	}
	return m_gizmo->is_modal() || m_panel->get_content_rect().contains(Platform::Input::get_mouse_position());
}

bool ViewportWindow::accepts_shortcuts(bool hovered) const {
	const bool transforming = m_gizmo != nullptr && m_gizmo->is_active();
	const bool navigating = Platform::Input::is_mouse_button_pressed(Platform::Events::MouseButton::Right);
	return (hovered || transforming) && !navigating;
}

void ViewportWindow::update(F32 delta_time) {
	Rendering::CameraController *camera = context().camera();
	if (camera == nullptr || m_panel == nullptr) {
		return;
	}

	const Rect viewport = m_panel->get_content_rect();
	if (viewport.size.x <= 1.F || viewport.size.y <= 1.F) {
		return;
	}

	const bool game_live = GameWindow::is_live(context());
	if (m_gizmo != nullptr) {
		m_gizmo->set_hidden(game_live);
	}
	if (m_overlay != nullptr) {
		m_overlay->set_hidden(game_live);
	}
	if (game_live) {
		camera->set_navigation_blocked(true);
		return;
	}

	camera->set_viewport_rect(viewport.position, viewport.size);
	camera->set_viewport_size(static_cast<Uint32>(viewport.size.x), static_cast<Uint32>(viewport.size.y));
	const UI::Core::View *surface = m_panel->get_view();
	const UI::Core::Canvas *canvas = surface != nullptr ? surface->get_canvas() : nullptr;
	const UI::Core::View *hovered = canvas != nullptr ? canvas->get_hovered_view() : nullptr;
	const bool over_surface =
		hovered == surface || (m_gizmo != nullptr && hovered == m_gizmo && !m_gizmo->is_over_handle());
	camera->set_navigation_blocked(!over_surface || m_view_camera.is_valid());
	camera->update(delta_time);
	const Rendering::RenderView view = current_view();
	context().engine().get_render_pipeline().set_primary_view(view);
	if (m_overlay != nullptr) {
		m_overlay->sync(viewport, view);
		refresh_debug_info(delta_time);
	}
	if (m_gizmo != nullptr) {
		m_gizmo->sync(viewport, view);
	}
}

void ViewportWindow::request_pick(Vec2 uv) {
	if (Platform::Input::is_key_pressed(Events::KeyCode::LeftAlt)) {
		return;
	}

	auto &output = context().engine().get_render_output();
	const F32 x = uv.x * static_cast<F32>(output.get_width());
	const F32 y = uv.y * static_cast<F32>(output.get_height());
	if (x < 0.F || y < 0.F) {
		return;
	}
	context().engine().get_object_picking().request_pick(static_cast<Uint32>(x), static_cast<Uint32>(y));
}

void ViewportWindow::wire_tools(UI::Core::View &content) {
	const std::array<std::pair<const char *, TransformTool>, 3> tools = { {
		{ "tool-translate", TransformTool::Translate },
		{ "tool-rotate", TransformTool::Rotate },
		{ "tool-scale", TransformTool::Scale },
	} };
	for (Usize i = 0; i < tools.size(); ++i) {
		m_tool_buttons[i] = content.find_by_id<UI::Core::Button>(tools[i].first);
		if (m_tool_buttons[i] != nullptr) {
			const TransformTool tool = tools[i].second;
			m_tool_buttons[i]->on_click.connect([this, tool] { context().tools.set_tool(tool); });
		}
	}

	m_space_button = content.find_by_id<UI::Core::Button>("tool-space");
	if (m_space_button != nullptr) {
		m_space_button->on_click.connect([this] { context().tools.toggle_space(); });
	}

	m_snap_button = content.find_by_id<UI::Core::Button>("tool-snap");
	if (m_snap_button != nullptr) {
		m_snap_button->on_click.connect([this] { context().tools.toggle_snapping(); });
	}

	listen(context().tools.on_tool_changed, [this](TransformTool) { refresh_tools(); });
	listen(context().tools.on_space_changed, [this](TransformSpace) { refresh_tools(); });
	listen(context().tools.on_snapping_changed, [this](bool) { refresh_tools(); });
	refresh_tools();
}

void ViewportWindow::wire_shading(UI::Core::View &content) {
	auto *shading = content.find_by_id<UI::Core::Dropdown>("viewport-shading");
	if (shading == nullptr) {
		return;
	}

	Rendering::RenderSettings &settings = context().engine().get_render_pipeline().get_settings();
	shading->set_variant("overlay-dropdown");
	shading->set_chevron(context().icon("chevron-down"));
	for (const ShadingMode &mode : k_shading_modes) {
		shading->add_option(std::to_string(static_cast<Uint32>(mode.view)), mode.label, context().icon(mode.icon));
	}
	shading->set_value(std::to_string(static_cast<Uint32>(settings.debug_view)));
	shading->on_changed.connect([&settings](const std::string &value) {
		settings.debug_view = static_cast<Rendering::DebugView>(std::stoul(value));
		Foundation::FrameScheduler::get()->request_frame();
	});
}

void ViewportWindow::wire_show_menu(UI::Core::View &content) {
	UI::Core::View *anchor = content.find_by_id("viewport-show");
	auto *button = content.find_by_id<UI::Core::Button>("viewport-show-button");
	if (anchor == nullptr || button == nullptr) {
		return;
	}

	Rendering::RenderSettings &settings = context().engine().get_render_pipeline().get_settings();
	auto *popup = anchor->add_child<UI::Core::Popup>();
	popup->add_class("overlay-popover");

	auto *title = popup->add_child<UI::Core::Label>(std::string("Show"));
	title->add_class("overlay-popover-title");

	for (const ShowToggle &toggle : k_show_toggles) {
		auto *row = popup->add_child<UI::Core::View>();
		row->add_class("overlay-check-row");
		auto *check = row->add_child<UI::Core::Checkbox>(settings.*toggle.flag);
		check->add_class("overlay-check");
		row->add_child<UI::Core::Label>(std::string(toggle.label))->add_class("overlay-check-text");
		check->on_changed.connect([&settings, flag = toggle.flag](const bool &enabled) {
			settings.*flag = enabled;
			Foundation::FrameScheduler::get()->request_frame();
		});
	}

	if (m_overlay != nullptr) {
		popup->add_child<UI::Core::Label>(std::string("Overlays"))->add_class("overlay-popover-title");
		const std::array<std::pair<const char *, bool ViewportOverlay::Options::*>, 6> overlays = { {
			{ "Axis Widget", &ViewportOverlay::Options::axis_widget },
			{ "Selection Label", &ViewportOverlay::Options::selection_label },
			{ "Safe Frame", &ViewportOverlay::Options::safe_frame },
			{ "Render Status", &ViewportOverlay::Options::render_status },
			{ "Statistics", &ViewportOverlay::Options::statistics },
			{ "GPU Timings", &ViewportOverlay::Options::gpu_timings },
		} };
		for (const auto &[label, flag] : overlays) {
			auto *row = popup->add_child<UI::Core::View>();
			row->add_class("overlay-check-row");
			auto *check = row->add_child<UI::Core::Checkbox>(m_overlay->options().*flag);
			check->add_class("overlay-check");
			row->add_child<UI::Core::Label>(std::string(label))->add_class("overlay-check-text");
			check->on_changed.connect([this, flag](const bool &enabled) {
				m_overlay->set_option(flag, enabled);
				m_debug_refresh_timer = 0.F;
				Foundation::FrameScheduler::get()->request_frame();
			});
		}
	}

	button->on_click.connect([popup] { popup->toggle(); });
}

void ViewportWindow::refresh_debug_info(F32 delta_time) {
	if (!m_overlay->wants_debug_info()) {
		return;
	}
	m_debug_refresh_timer -= delta_time;
	if (m_debug_refresh_timer > 0.F) {
		return;
	}
	constexpr F32 k_refresh_seconds = 0.25F;
	m_debug_refresh_timer = k_refresh_seconds;

	Rendering::RenderPipeline &pipeline = context().engine().get_render_pipeline();
	ViewportOverlay::DebugInfo info;
	const Rendering::DebugView mode = pipeline.get_settings().debug_view;
	for (const ShadingMode &shading : k_shading_modes) {
		if (shading.view == mode) {
			info.shading = shading.label;
		}
	}
	info.width = pipeline.get_width();
	info.height = pipeline.get_height();
	info.cpu_ms = static_cast<F32>(Foundation::Profiler::get()->get_frame_duration());
	info.has_gpu = pipeline.has_gpu_timings();
	info.gpu_ms = pipeline.get_gpu_frame_milliseconds();
	info.passes = pipeline.get_pass_timings();
	info.statistics = m_statistics.collect(context().engine().get_scene(), context().selection.get());
	m_overlay->set_debug_info(std::move(info));
}

Rendering::RenderView ViewportWindow::current_view() const {
	Rendering::CameraController *camera = context().camera();
	Entity scene_camera = m_view_camera;
	if (scene_camera.is_valid() && scene_camera.exists() &&
		scene_camera.has_component<SceneManagement::Components::CameraComponent>() &&
		scene_camera.has_component<SceneManagement::Components::TransformComponent>()) {
		const auto &lens = scene_camera.get_component<SceneManagement::Components::CameraComponent>();
		auto &transform = scene_camera.get_component<SceneManagement::Components::TransformComponent>();
		Rendering::RenderView view =
			camera->render_view_from(transform.get_world_matrix_lazy(), lens.fov, lens.near_plane, lens.far_plane);
		view.ev100 = lens.ev100;
		view.exposure_compensation = lens.exposure_compensation;
		view.auto_exposure = lens.auto_exposure;
		return view;
	}
	return camera->get_render_view();
}

void ViewportWindow::wire_camera_menu(UI::Core::View &content) {
	UI::Core::View *anchor = content.find_by_id("viewport-camera");
	m_camera_button = content.find_by_id<UI::Core::Button>("viewport-camera-button");
	if (anchor == nullptr || m_camera_button == nullptr) {
		return;
	}

	m_camera_popup = anchor->add_child<UI::Core::Popup>();
	m_camera_popup->add_class("overlay-popover");
	m_camera_popup->add_class("overlay-popover-wide");
	m_camera_popup->add_child<UI::Core::Label>(std::string("Camera"))->add_class("overlay-popover-title");

	auto *grid = m_camera_popup->add_child<UI::Core::PropertyGrid>();
	grid->set_split(true);

	m_fov_field = grid->add_row<UI::Core::DragFloat>(
		"Field of View", UI::Core::DragFloat::Config{ .min = 10.F, .max = 120.F, .speed = 0.25F, .precision = 0 });
	m_fov_field->set_slider(true);
	m_fov_field->set_suffix("°");
	m_fov_field->on_changed.connect([this](float value) {
		if (Rendering::CameraController *camera = context().camera()) {
			camera->set_fov(value);
		}
	});

	m_speed_field = grid->add_row<UI::Core::DragFloat>(
		"Speed", UI::Core::DragFloat::Config{ .min = 0.1F, .max = 100.F, .speed = 0.05F, .precision = 1 });
	m_speed_field->set_suffix("m/s");
	m_speed_field->on_changed.connect([this](float value) {
		if (Rendering::CameraController *camera = context().camera()) {
			camera->set_move_speed(value);
		}
	});

	m_auto_exposure_check = grid->add_check_row<UI::Core::Checkbox>("Auto Exposure", true);
	m_auto_exposure_check->on_changed.connect([this](const bool &enabled) {
		if (Rendering::CameraController *camera = context().camera()) {
			camera->set_auto_exposure(enabled);
		}
		refresh_exposure_rows();
	});

	m_compensation_field = grid->add_row<UI::Core::DragFloat>(
		"Compensation", UI::Core::DragFloat::Config{ .min = -8.F, .max = 8.F, .speed = 0.02F, .precision = 1 });
	m_compensation_field->set_suffix("EV");
	m_compensation_field->on_changed.connect([this](float value) {
		if (Rendering::CameraController *camera = context().camera()) {
			camera->set_exposure_compensation(value);
		}
	});

	m_exposure_field = grid->add_row<UI::Core::DragFloat>(
		"Exposure", UI::Core::DragFloat::Config{ .min = -10.F, .max = 24.F, .speed = 0.05F, .precision = 1 });
	m_exposure_field->set_suffix("EV");
	m_exposure_field->on_changed.connect([this](float value) {
		if (Rendering::CameraController *camera = context().camera()) {
			camera->set_ev100(value);
		}
	});

	Rendering::RenderSettings &settings = context().engine().get_render_pipeline().get_settings();
	m_tone_mapper = grid->add_row<UI::Core::Dropdown>("Tone Mapping");
	m_tone_mapper->set_variant("props-dropdown");
	m_tone_mapper->set_chevron(context().icon("chevron-down"));
	for (const ToneMapperOption &option : k_tone_mappers) {
		m_tone_mapper->add_option(std::to_string(static_cast<Uint32>(option.mapper)), option.label);
	}
	m_tone_mapper->on_changed.connect([&settings](const std::string &value) {
		settings.tone_mapper = static_cast<Rendering::ToneMapper>(std::stoul(value));
		Foundation::FrameScheduler::get()->request_frame();
	});

	m_view_through = grid->add_row<UI::Core::Dropdown>("View Through");
	m_view_through->set_variant("props-dropdown");
	m_view_through->set_chevron(context().icon("chevron-down"));
	m_view_through->on_changed.connect([this](const std::string &value) {
		const Usize index = static_cast<Usize>(std::stoul(value));
		m_view_camera = index == 0 || index > m_scene_cameras.size() ? Entity{} : m_scene_cameras[index - 1];
		m_camera_button->set_class("overlay-tool-active", m_view_camera.is_valid());
		m_fov_field->set_enabled(!m_view_camera.is_valid());
		m_speed_field->set_enabled(!m_view_camera.is_valid());
		m_auto_exposure_check->set_enabled(!m_view_camera.is_valid());
		m_compensation_field->set_enabled(!m_view_camera.is_valid());
		m_exposure_field->set_enabled(!m_view_camera.is_valid());
		Foundation::FrameScheduler::get()->request_frame();
	});

	m_camera_button->on_click.connect([this] {
		if (!m_camera_popup->is_open()) {
			refresh_camera_menu();
		}
		m_camera_popup->toggle();
	});
}

void ViewportWindow::refresh_camera_menu() {
	if (Rendering::CameraController *camera = context().camera()) {
		m_fov_field->set_value(camera->get_fov());
		m_speed_field->set_value(camera->get_move_speed());
		m_exposure_field->set_value(camera->get_ev100());
		m_compensation_field->set_value(camera->get_exposure_compensation());
		m_auto_exposure_check->set_checked(camera->get_auto_exposure());
	}
	refresh_exposure_rows();
	const Rendering::ToneMapper tone_mapper = context().engine().get_render_pipeline().get_settings().tone_mapper;
	m_tone_mapper->set_value(std::to_string(static_cast<Uint32>(tone_mapper)));

	m_scene_cameras.clear();
	context().engine().get_scene().get_entity_manager()->for_each<SceneManagement::Components::CameraComponent>(
		[this](Entity entity) { m_scene_cameras.push_back(entity); });

	m_view_through->clear_options();
	m_view_through->add_option("0", "Editor Camera", context().icon("move-3d"));
	std::string selected = "0";
	for (Usize i = 0; i < m_scene_cameras.size(); ++i) {
		const std::string value = std::to_string(i + 1);
		m_view_through->add_option(value, m_scene_cameras[i].get_name(), context().icon("video"));
		if (m_scene_cameras[i] == m_view_camera) {
			selected = value;
		}
	}
	m_view_through->set_value(selected);
}

void ViewportWindow::refresh_exposure_rows() {
	const bool automatic = m_auto_exposure_check != nullptr && m_auto_exposure_check->is_checked();
	if (m_compensation_field != nullptr && m_compensation_field->get_parent() != nullptr) {
		m_compensation_field->get_parent()->set_hidden(!automatic);
	}
	if (m_exposure_field != nullptr && m_exposure_field->get_parent() != nullptr) {
		m_exposure_field->get_parent()->set_hidden(automatic);
	}
}

void ViewportWindow::refresh_tools() {
	for (Usize i = 0; i < m_tool_buttons.size(); ++i) {
		if (m_tool_buttons[i] != nullptr) {
			m_tool_buttons[i]->set_class("overlay-tool-active", static_cast<Usize>(context().tools.get_tool()) == i);
		}
	}
	if (m_space_button != nullptr) {
		const bool world = context().tools.get_space() == TransformSpace::World;
		m_space_button->set_icon(context().icon(world ? "globe" : "cuboid"));
		m_space_button->set_tooltip(world ? "World space (click for local)" : "Local space (click for world)");
	}
	if (m_snap_button != nullptr) {
		m_snap_button->set_class("overlay-tool-active", context().tools.is_snapping());
	}
}

} // namespace Editor
