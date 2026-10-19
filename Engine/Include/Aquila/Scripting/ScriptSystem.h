#ifndef AQUILA_SCRIPTING_SCRIPT_SYSTEM_H
#define AQUILA_SCRIPTING_SCRIPT_SYSTEM_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scene/Components/ScriptComponent.h"
#include "Aquila/Scripting/ScriptBindings.h"

#include "runtime.h"

namespace Aquila::Scripting {

class ScriptSystem {
  public:
	/// `input` is what Aquila.Input reads, null for no input. It must outlive the system.
	/// Script errors, with a Luau traceback, go to the engine log (so the editor's console).
	explicit ScriptSystem(SceneManagement::Scene &scene, const Platform::InputState *input = nullptr);
	~ScriptSystem();
	AQUILA_NONCOPYABLE(ScriptSystem);
	AQUILA_NONMOVEABLE(ScriptSystem);

	/// Advances Aquila.Time, starts scripts on new or changed paths, stops removed ones, then calls on_update.
	/// A script that errors stops being called until its path changes.
	void update(F32 delta_time);
	/// Calls on_stop on every running script and unloads them all. The next update() starts them again.
	void stop();

	/// Luau type definitions of the engine API, for autocomplete in an editor.
	bool write_types(const std::string &directory) const;

  private:
	struct Instance {
		std::string path;
		aquiLua::ScriptId script;
		bool has_update = false;
		bool has_stop = false;
		bool failed = false;
	};

	void start(entt::entity entity, Instance &instance, const SceneManagement::Components::ScriptComponent &component);
	void finish(entt::entity entity, Instance &instance);

	SceneManagement::Scene &m_scene;
	ScriptEnvironment m_environment;
	aquiLua::Runtime m_runtime;
	std::unordered_map<entt::entity, Instance> m_instances;
};

} // namespace Aquila::Scripting

#endif
