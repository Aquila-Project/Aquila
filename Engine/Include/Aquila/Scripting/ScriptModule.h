#ifndef AQUILA_SCRIPTING_SCRIPT_MODULE_H
#define AQUILA_SCRIPTING_SCRIPT_MODULE_H

#include "Aquila/Application/IModule.h"
#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Scripting {

class ScriptSystem;

/// Runs every ScriptComponent in the scene while the game plays, never at edit time. Every play starts the
/// scripts fresh, in a new Luau VM. Writes the Luau type definitions on attach.
class ScriptModule final : public Application::IModule {
  public:
	ScriptModule();
	~ScriptModule() override;

	void on_attach(Application::EngineContext &engine) override;
	void on_detach() override;
	void on_pre_render(F32 delta_time) override;
	void on_play() override;
	void on_stop() override;

  private:
	Application::EngineContext *m_engine = nullptr;
	Unique<ScriptSystem> m_system;
};

} // namespace Aquila::Scripting

#endif
