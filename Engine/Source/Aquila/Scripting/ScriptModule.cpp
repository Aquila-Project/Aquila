#include "Aquila/Scripting/ScriptModule.h"

#include "Aquila/Application/EngineContext.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Scripting/ScriptComponent.h"
#include "Aquila/Scripting/ScriptSystem.h"

namespace Aquila::Scripting {

ScriptModule::ScriptModule() {
	register_script_component();
}

ScriptModule::~ScriptModule() = default;

void ScriptModule::on_attach(Application::EngineContext &engine) {
	m_system = std::make_unique<ScriptSystem>(engine.get_scene());
	m_system->write_types(SharedConstants::RESOURCES_DIR + "Engine/Types");
}

void ScriptModule::on_detach() {
	m_system.reset();
}

void ScriptModule::on_pre_render(F32 delta_time) {
	if (m_system) {
		m_system->update(delta_time);
	}
}

} // namespace Aquila::Scripting
