#include "Aquila/Application/EngineContext.h"

#include "Aquila/Application/ApplicationNew.h"
#include "Aquila/Rendering/Systems/ObjectPickingSystem.h"

namespace Aquila::Application {

Window &EngineContext::get_window() const {
	return m_application.get_window();
}

GFX::GfxContext &EngineContext::get_context() const {
	return m_application.get_context();
}

SceneManagement::Scene &EngineContext::get_scene() const {
	return m_application.get_scene();
}

Rendering::RenderPipeline &EngineContext::get_render_pipeline() const {
	return m_application.get_render_pipeline();
}

Rendering::Renderer &EngineContext::get_renderer() const {
	return m_application.get_renderer();
}

Rendering::OverlayRenderer &EngineContext::get_overlay_renderer() const {
	return m_application.get_overlay_renderer();
}

Rendering::ObjectPickingSystem &EngineContext::get_object_picking() const {
	return m_application.get_object_picking();
}

GFX::GfxTexture &EngineContext::get_render_output() const {
	return m_application.get_render_output();
}

Rendering::IRenderWindowHost &EngineContext::get_window_host() const {
	return m_application;
}

void EngineContext::request_render_resize(Uint32 width, Uint32 height) const {
	m_application.request_render_resize(width, height);
}

void EngineContext::request_close() const {
	m_application.close();
}

void EngineContext::play() const {
	if (m_application.m_play_state == PlayState::Stopped) {
		m_application.set_play_state(PlayState::Playing);
	}
}

void EngineContext::pause() const {
	if (m_application.m_play_state == PlayState::Playing) {
		m_application.set_play_state(PlayState::Paused);
	}
}

void EngineContext::resume() const {
	if (m_application.m_play_state == PlayState::Paused) {
		m_application.set_play_state(PlayState::Playing);
	}
}

void EngineContext::stop() const {
	if (m_application.m_play_state != PlayState::Stopped) {
		m_application.set_play_state(PlayState::Stopped);
	}
}

PlayState EngineContext::get_play_state() const {
	return m_application.m_play_state;
}

}
