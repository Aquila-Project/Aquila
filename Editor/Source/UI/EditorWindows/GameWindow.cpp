#include "UI/EditorWindows/GameWindow.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Platform/InputState.h"
#include "Aquila/Rendering/CameraController.h"
#include "Aquila/Rendering/RenderPipeline.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/Label.h"

namespace Editor {

using namespace Aquila;
using Application::PlayState;

GameWindow::GameWindow(EditorContext &context) : EditorWindow(context) {}

GameWindow::~GameWindow() {
	context().set_game_view(std::nullopt);
	set_live(false);
}

void GameWindow::build(UI::Core::View &content) {
	if (auto layout = load_layout("game.aqlayout")) {
		content.add_child(std::move(layout));
	}
	m_image = content.find_by_id<UI::Core::Image>("game-image");
	m_stage = content.find_by_id("game-stage");
	m_idle = content.find_by_id("game-idle");
	m_no_camera = content.find_by_id("game-no-camera");
	m_camera_name = content.find_by_id<UI::Core::Label>("game-camera");
	m_pause_button = content.find_by_id<UI::Core::Button>("game-pause");
	if (m_image == nullptr || m_stage == nullptr || m_idle == nullptr || m_no_camera == nullptr || m_camera_name == nullptr ||
		m_pause_button == nullptr) {
		AQUILA_LOG_ERROR("GameWindow: game.aqlayout is missing one of its ids");
		m_image = nullptr;
		return;
	}

	m_image->set_texture(&context().engine().get_render_output());
	listen(context().on_render_output_changed,
		   [this] { m_image->set_texture(&context().engine().get_render_output()); });
	m_pause_button->on_click.connect([this] { context().shortcuts.execute("play.pause"); });
}

bool GameWindow::is_live(const EditorContext &context) {
	return context.engine().get_play_state() != PlayState::Stopped && context.is_window_visible(k_type_id);
}

void GameWindow::update(F32) {
	if (m_image == nullptr) {
		return;
	}

	const PlayState state = context().engine().get_play_state();
	SceneManagement::Scene &scene = context().engine().get_scene();
	const bool playing = state != PlayState::Stopped;
	const bool has_camera = playing && scene.has_active_camera();

	m_image->set_hidden(!has_camera);
	m_idle->set_hidden(playing);
	m_no_camera->set_hidden(!playing || has_camera);
	m_pause_button->set_enabled(playing);
	m_pause_button->set_class("topbar-btn-active", state == PlayState::Paused);
	m_camera_name->set_text(has_camera ? scene.get_active_camera_entity().get_name() : std::string());

	const bool live = is_live(context());
	set_live(live);
	if (!live) {
		context().set_game_view(std::nullopt);
		return;
	}

	const Rect view = has_camera ? m_image->get_absolute_rect() : m_stage->get_absolute_rect();
	context().set_game_view(view);
	context().engine().get_game_input().set_view(view.position, view.size);
	m_stage->set_class("game-stage-focused", context().is_game_focused());

	if (Rendering::CameraController *camera = context().camera()) {
		camera->set_navigation_blocked(true);
	}
	context().engine().get_render_pipeline().clear_primary_view();

	if (has_camera && view.size.x > 1.F && view.size.y > 1.F) {
		auto &lens = scene.get_active_camera_entity().get_component<SceneManagement::Components::CameraComponent>();
		if (lens.aspect_ratio != view.size.x / view.size.y) {
			lens.on_resize(view.size.x, view.size.y);
		}
	}
}

void GameWindow::set_live(bool live) {
	if (live == m_editor_settings.has_value()) {
		return;
	}
	Rendering::RenderSettings &settings = context().engine().get_render_pipeline().get_settings();
	if (live) {
		m_editor_settings = settings;
		settings = game_render_settings(context().project().settings);
	} else {
		settings = *m_editor_settings;
		m_editor_settings.reset();
	}
	Foundation::FrameScheduler::get()->request_frame();
}

}
