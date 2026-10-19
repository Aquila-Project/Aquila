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

/// What scripts see as `Entity`, just the EnTT id (index and version, so a recycled id doesn't match).
struct ScriptEntity {
	Uint32 id;
};

/// What the engine API reads. The ScriptSystem owns it, keeps it current and must outlive the runtime.
struct ScriptEnvironment {
	SceneManagement::Scene *scene = nullptr;
	/// Null reads as nothing held and a zero sized screen.
	const Platform::InputState *input = nullptr;
	F32 delta_time = 0.F;
	F64 elapsed = 0.0;
	Uint64 frame = 0;
};

/// Binds the internal native modules (`@native`, `@input`, `@time`, `@scene`, `@screen`, `@camera`, `@fields`) that
/// Resources/Engine/API wraps as `@aquila` (Aquila.Input, Aquila.Time, Aquila.Scene, Aquila.Screen, Aquila.Camera,
/// Aquila.Field, Aquila.Log), and the Entity methods.
void register_engine_api(aquiLua::Runtime &runtime, ScriptEnvironment &environment);

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

/// `vector?`: nil or a vector.
template <> struct aquiLua::Converter<Option<Vec3>> {
	static constexpr const char *luau_type = "vector?";

	static void push(lua_State *state, const Option<Vec3> &value) {
		if (value) {
			aquiLua::Converter<Vec3>::push(state, *value);
		} else {
			lua_pushnil(state);
		}
	}

	static Option<Vec3> check(lua_State *state, int index) {
		if (lua_isnoneornil(state, index)) {
			return std::nullopt;
		}
		return aquiLua::Converter<Vec3>::check(state, index);
	}

	static Option<Vec3> get(lua_State *state, int index) {
		return lua_isvector(state, index) ? check(state, index) : std::nullopt;
	}
};

/// `Entity?`: nil or an Entity.
template <> struct aquiLua::Converter<Option<Aquila::Scripting::ScriptEntity>> {
	static constexpr const char *luau_type = "Entity?";

	static void push(lua_State *state, const Option<Aquila::Scripting::ScriptEntity> &value) {
		if (value) {
			aquiLua::push(state, *value);
		} else {
			lua_pushnil(state);
		}
	}

	static Option<Aquila::Scripting::ScriptEntity> check(lua_State *state, int index) {
		if (lua_isnoneornil(state, index)) {
			return std::nullopt;
		}
		return aquiLua::check<Aquila::Scripting::ScriptEntity>(state, index);
	}

	static Option<Aquila::Scripting::ScriptEntity> get(lua_State *state, int index) {
		return lua_isuserdata(state, index) ? check(state, index) : std::nullopt;
	}
};

#endif
