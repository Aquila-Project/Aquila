#ifndef AQUILA_SCRIPTING_SCRIPT_API_H
#define AQUILA_SCRIPTING_SCRIPT_API_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/Registration.h"
#include "Aquila/Scripting/ScriptBindings.h"

namespace Aquila::Scripting {

/// The Luau API scripts get: the engine's modules, then the ones other modules add. ScriptModule provides it as a
/// service, so a module added after it finds it in on_attach. Main thread only.
///
/// Scripts can't require a native module, only Luau files loaded through an alias can. So bind the natives under
/// one name and add an alias with another one, a real folder holding the Luau module scripts require:
/// @code
/// m_lua = services.find<ScriptApi>()->add([this](aquiLua::Runtime &runtime, ScriptEnvironment &) {
///     runtime.bind<raycast>("physics_native.raycast", &m_world, "First hit along the ray.");
///     runtime.add_alias("physics", m_luau_dir);
/// });
/// @endcode
class ScriptApi {
  public:
	using Binder = Delegate<void(aquiLua::Runtime &runtime, ScriptEnvironment &environment)>;

	ScriptApi() = default;
	AQUILA_NONCOPYABLE(ScriptApi);
	AQUILA_NONMOVEABLE(ScriptApi);

	/// Every runtime made while it's registered runs it: the next Play, the editor reading script fields, the type
	/// files. A game that's already playing keeps what it started with.
	[[nodiscard]] Foundation::Registration add(Binder binder);
	/// The engine's modules, then every binder in the order they were added.
	void bind(aquiLua::Runtime &runtime, ScriptEnvironment &environment) const;
	/// Moves on every add and remove, ScriptModule writes the type files again when it does.
	[[nodiscard]] Uint64 get_revision() const { return m_revision; }

  private:
	struct Entry {
		Uint64 id = 0;
		Binder binder;
	};

	std::vector<Entry> m_binders;
	Uint64 m_next_id = 0;
	Uint64 m_revision = 0;
	Foundation::RegistrationOwner m_owner;
};

}

#endif
