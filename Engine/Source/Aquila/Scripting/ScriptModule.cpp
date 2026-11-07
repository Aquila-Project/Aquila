#include "Aquila/Scripting/ScriptModule.h"
#include "Aquila/Foundation/EnginePaths.h"

#include "Aquila/Application/EngineContext.h"
#include "Aquila/Foundation/ServiceRegistry.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Scene/Components/ScriptComponent.h"
#include "Aquila/Scripting/ScriptSystem.h"

namespace Aquila::Scripting {

ScriptModule::ScriptModule() {
	SceneManagement::Components::register_script_component();
}

ScriptModule::~ScriptModule() = default;

void ScriptModule::on_attach(Application::EngineContext &engine) {
	m_engine = &engine;
	m_api_service = engine.get_services().provide<ScriptApi>(m_api);
}

void ScriptModule::on_detach() {
	m_system.reset();
	m_api_service.reset();
}

void ScriptModule::on_pre_render(F32 delta_time) {
	if (m_types_revision != m_api.get_revision()) {
		m_types_revision = m_api.get_revision();
		ScriptSystem(m_engine->get_scene(), nullptr, &m_api)
			.write_types(Foundation::EnginePaths::resources_dir() + "Engine/Types");
	}
	if (m_system && m_engine->get_play_state() == Application::PlayState::Playing) {
		m_system->update(delta_time);
		if (m_system->wants_quit()) {
			m_engine->stop();
		}
	}
}

void ScriptModule::on_play() {
	m_system = std::make_unique<ScriptSystem>(m_engine->get_scene(), &m_engine->get_game_input(), &m_api);
}

void ScriptModule::on_stop() {
	if (m_system) {
		m_system->stop();
	}
	m_system.reset();
}

} // namespace Aquila::Scripting
