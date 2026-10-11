#include "Aquila/Scripting/ScriptBindings.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/Scene.h"

namespace Aquila::Scripting {

namespace {

using SceneManagement::Components::MetadataComponent;
using SceneManagement::Components::TransformComponent;

void log(const std::string &message) {
	AQUILA_LOG_INFO("[Luau] {}", message);
}

void request_frame() {
	Foundation::FrameScheduler::get()->request_frame();
}

template <typename T> T *check_component(lua_State *state) {
	auto handle = aquiLua::check<ScriptEntity>(state, 1);
	auto &registry = aquiLua::context<SceneManagement::Scene>(state).get_registry();
	auto entity = static_cast<entt::entity>(handle.id);
	if (!registry.valid(entity)) {
		luaL_error(state, "entity %u was destroyed", handle.id);
	}
	T *component = registry.try_get<T>(entity);
	if (component == nullptr) {
		luaL_error(state, "entity %u is missing a component this method needs", handle.id);
	}
	return component;
}

} // namespace

void register_engine_api(aquiLua::Runtime &runtime, SceneManagement::Scene &scene) {
	runtime.bind<log>("native.log", "Writes message to the engine log.");
	runtime.bind<request_frame>("native.request_frame", "Requests another frame.");
	runtime.add_alias("aquila", SharedConstants::RESOURCES_DIR + "Engine/API");

	runtime.register_type<ScriptEntity>(
		{
			aquiLua::make_method<check_component<MetadataComponent>, &MetadataComponent::get_name>(
				"get_name", "Returns the name shown in the editor."),
			aquiLua::make_method<check_component<MetadataComponent>, &MetadataComponent::set_name>(
				"set_name", "Renames the entity."),
			aquiLua::make_method<check_component<TransformComponent>, &TransformComponent::get_local_position>(
				"get_position", "Returns the position, relative to the parent."),
			aquiLua::make_method<check_component<TransformComponent>, &TransformComponent::set_local_position>(
				"set_position", "Moves the entity, relative to the parent."),
			aquiLua::make_method<check_component<TransformComponent>, &TransformComponent::get_local_rotation_euler>(
				"get_rotation", "Returns the rotation as Euler angles in degrees, relative to the parent."),
			aquiLua::make_method<check_component<TransformComponent>, &TransformComponent::set_local_rotation_euler>(
				"set_rotation", "Sets the rotation from Euler angles in degrees, relative to the parent."),
			aquiLua::make_method<check_component<TransformComponent>, &TransformComponent::get_local_scale>(
				"get_scale", "Returns the scale, relative to the parent."),
			aquiLua::make_method<check_component<TransformComponent>, &TransformComponent::set_local_scale>(
				"set_scale", "Sets the scale, relative to the parent."),
		},
		&scene, "An entity in the scene. A script gets the entity it is attached to as self.");
}

} // namespace Aquila::Scripting
