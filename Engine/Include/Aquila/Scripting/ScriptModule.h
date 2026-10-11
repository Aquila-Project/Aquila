#ifndef AQUILA_SCRIPTING_SCRIPT_MODULE_H
#define AQUILA_SCRIPTING_SCRIPT_MODULE_H

#include "Aquila/Application/IModule.h"
#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Scripting {

class ScriptSystem;

class ScriptModule final : public Application::IModule {
  public:
	ScriptModule();
	~ScriptModule() override;

	void on_attach(Application::EngineContext &engine) override;
	void on_detach() override;
	void on_pre_render(F32 delta_time) override;

  private:
	Unique<ScriptSystem> m_system;
};

} // namespace Aquila::Scripting

#endif
