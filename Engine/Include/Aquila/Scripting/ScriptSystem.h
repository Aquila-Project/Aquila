#ifndef AQUILA_SCRIPTING_SCRIPT_SYSTEM_H
#define AQUILA_SCRIPTING_SCRIPT_SYSTEM_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Scene/Scene.h"

#include "runtime.h"

#include <string>
#include <unordered_map>

namespace Aquila::Scripting {

class ScriptSystem {
  public:
	explicit ScriptSystem(SceneManagement::Scene &scene);
	~ScriptSystem();
	AQUILA_NONCOPYABLE(ScriptSystem);
	AQUILA_NONMOVEABLE(ScriptSystem);

	void update(F32 delta_time);

	bool write_types(const std::string &directory) const;

  private:
	struct Instance {
		std::string path;
		aquiLua::ScriptId script;
		bool has_update = false;
		bool failed = false;
	};

	void start(entt::entity entity, Instance &instance, const std::string &path);

	SceneManagement::Scene &m_scene;
	aquiLua::Runtime m_runtime;
	std::unordered_map<entt::entity, Instance> m_instances;
};

} // namespace Aquila::Scripting

#endif
