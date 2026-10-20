#include "Aquila/Player/PlayerModule.h"

#include "Aquila/Application/EngineContext.h"
#include "Aquila/Application/Window.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Graphics/Texture/TextureLibrary.h"
#include "Aquila/Platform/Filesystem/NativeFileSystem.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Platform/InputState.h"
#include "Aquila/Rendering/RenderPipeline.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/DefaultScenes.h"
#include "Aquila/Scene/Entity.h"
#include "Aquila/Scene/Scene.h"

namespace Aquila::Player {

using SceneManagement::Components::CameraComponent;

PlayerModule::PlayerModule(BuildLayout layout, BuildManifest manifest)
	: m_layout(std::move(layout)), m_manifest(std::move(manifest)) {}

void PlayerModule::on_attach(Application::EngineContext &engine) {
	m_engine = &engine;

	Rendering::RenderSettings &settings = engine.get_render_pipeline().get_settings();
	settings = Rendering::RenderSettings::game();
	settings.tone_mapper = m_manifest.tone_mapper;
	settings.show_shadows = m_manifest.shadows;

	auto *vfs = Platform::Filesystem::VirtualFileSystem::get();
	if (vfs == nullptr) {
		AQUILA_LOG_ERROR("PlayerModule: Virtual file system is unavailable.");
		engine.request_close();
		return;
	}

	auto native_fs = std::make_shared<Platform::Filesystem::NativeFileSystem>(m_layout.project_dir());
	if (!vfs->mount(SharedConstants::PROJECT_MOUNT, native_fs)) {
		AQUILA_LOG_ERROR("PlayerModule: Failed to mount project directory '{}' at '{}'.", m_layout.project_dir(),
						 SharedConstants::PROJECT_MOUNT);
		engine.request_close();
		return;
	}

	SceneManagement::Scene &scene = engine.get_scene();
	const std::string scene_path = std::string{ SharedConstants::PROJECT_SCENES_DIR } + "/" + m_manifest.startup_scene;
	if (!scene.deserialize(scene_path)) {
		AQUILA_LOG_ERROR("PlayerModule: Failed to deserialize startup scene from '{}'.", scene_path);
		engine.request_close();
		return;
	}
	Graphics::TextureLibrary::get()->finish_loading(engine.get_context());

	SceneManagement::Entity camera = scene.find_game_camera();
	if (camera.is_valid() && !camera.get_component<CameraComponent>().primary) {
		AQUILA_LOG_WARNING("PlayerModule: '{}' has no primary camera, using the first one.", scene_path);
	}
	if (camera.is_valid()) {
		scene.set_active_camera(camera);
	} else {
		AQUILA_LOG_WARNING("PlayerModule: '{}' has no camera, adding the default one.", scene_path);
		SceneManagement::spawn_default_camera(scene, 1.F);
	}

	const Application::Window &window = engine.get_window();
	fit_camera(window.get_width(), window.get_height());
	engine.play();
}

void PlayerModule::on_event(Platform::Events::Event &event) {
	if (m_engine != nullptr) {
		m_engine->get_game_input().on_event(event);
	}
}

void PlayerModule::on_render_resize(Uint32 width, Uint32 height) {
	fit_camera(width, height);
}

void PlayerModule::fit_camera(Uint32 width, Uint32 height) const {
	if (m_engine == nullptr || width == 0 || height == 0) {
		return;
	}
	m_engine->get_game_input().set_view(Vec2(0.F), Vec2(static_cast<F32>(width), static_cast<F32>(height)));
	SceneManagement::Scene &scene = m_engine->get_scene();
	if (!scene.has_active_camera()) {
		return;
	}
	scene.get_active_camera_entity().get_component<CameraComponent>().on_resize(static_cast<F32>(width),
																				static_cast<F32>(height));
}

} // namespace Aquila::Player
