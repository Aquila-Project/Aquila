#ifndef AQUILA_SCRIPTING_SCRIPT_SYSTEM_H
#define AQUILA_SCRIPTING_SCRIPT_SYSTEM_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scene/Components/ScriptComponent.h"

#include "runtime.h"

namespace Aquila::Scripting {

class ScriptSystem {
  public:
	explicit ScriptSystem(SceneManagement::Scene &scene);
	~ScriptSystem();
	AQUILA_NONCOPYABLE(ScriptSystem);
	AQUILA_NONMOVEABLE(ScriptSystem);

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

	void finish(entt::entity entity, Instance &instance);

	SceneManagement::Scene &m_scene;
	aquiLua::Runtime m_runtime;
	std::unordered_map<entt::entity, Instance> m_instances;
};

} // namespace Aquila::Scripting

#endif
