#ifndef AQUILA_SCRIPTING_SCRIPT_BINDINGS_H
#define AQUILA_SCRIPTING_SCRIPT_BINDINGS_H

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/PrimitiveTypes.h"

#include "runtime.h"

namespace Aquila::SceneManagement {
class Scene;
}

namespace Aquila::Platform {
class InputState;
}

namespace Aquila::Scripting {

/// What scripts see as `Entity`, just the EnTT id.
struct ScriptEntity {
	Uint32 id;
};

/// Exposes `@native` (log, request_frame), the `@aquila` module and the Entity methods to scripts.
void register_engine_api(aquiLua::Runtime &runtime, SceneManagement::Scene &scene);

} // namespace Aquila::Scripting

AQUILUA_USERTYPE(Aquila::Scripting::ScriptEntity, "Entity");

template <> struct aquiLua::Converter<Vec3> {
	static constexpr const char *luau_type = "vector";

	static void push(lua_State *state, const Vec3 &value) { lua_pushvector(state, value.x, value.y, value.z); }

	static Vec3 check(lua_State *state, int index) {
		const float *value = luaL_checkvector(state, index);
		return Vec3{ value[0], value[1], value[2] };
	}

	static Vec3 get(lua_State *state, int index) {
		const float *value = lua_tovector(state, index);
		return value != nullptr ? Vec3{ value[0], value[1], value[2] } : Vec3{ 0.F };
	}
};

#endif
