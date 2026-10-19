#include "Aquila/Scripting/ScriptBindings.h"
#include "Aquila/Foundation/EnginePaths.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Platform/Events/KeyChord.h"
#include "Aquila/Platform/InputState.h"
#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/Rendering/ViewMath.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Scene/Components/SceneNodeComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Scene/Scene.h"

template <> struct aquiLua::Converter<Aquila::Platform::Events::KeyCode> {
	using KeyCode = Aquila::Platform::Events::KeyCode;

	static constexpr const char *luau_type = "string";

	static Option<KeyCode> from_name(std::string_view name) {
		constexpr std::array<std::pair<std::string_view, KeyCode>, 6> k_sides{ {
			{ "LeftShift", KeyCode::LeftShift },
			{ "RightShift", KeyCode::RightShift },
			{ "LeftCtrl", KeyCode::LeftControl },
			{ "RightCtrl", KeyCode::RightControl },
			{ "LeftAlt", KeyCode::LeftAlt },
			{ "RightAlt", KeyCode::RightAlt },
		} };
		for (const auto &[side, key] : k_sides) {
			if (side == name) {
				return key;
			}
		}
		return Aquila::Platform::Events::key_from_name(name);
	}

	static void push(lua_State *state, KeyCode key) { aquiLua::push(state, Aquila::Platform::Events::key_name(key)); }

	static KeyCode check(lua_State *state, int index) {
		const char *name = luaL_checkstring(state, index);
		if (const Option<KeyCode> key = from_name(name)) {
			return *key;
		}
		luaL_error(state, "unknown key '%s'", name);
	}

	static KeyCode get(lua_State *state, int index) {
		const char *name = lua_tostring(state, index);
		return name != nullptr ? from_name(name).value_or(KeyCode::Unknown) : KeyCode::Unknown;
	}
};

template <> struct aquiLua::Converter<Aquila::Platform::Events::MouseButton> {
	using MouseButton = Aquila::Platform::Events::MouseButton;

	static constexpr const char *luau_type = "string";
	static constexpr std::array<std::pair<std::string_view, MouseButton>, 3> k_names{ {
		{ "Left", MouseButton::Left },
		{ "Right", MouseButton::Right },
		{ "Middle", MouseButton::Middle },
	} };

	static void push(lua_State *state, MouseButton button) {
		for (const auto &[name, value] : k_names) {
			if (value == button) {
				aquiLua::push(state, name);
				return;
			}
		}
		lua_pushnil(state);
	}

	static MouseButton check(lua_State *state, int index) {
		const std::string_view name = luaL_checkstring(state, index);
		for (const auto &[button_name, button] : k_names) {
			if (button_name == name) {
				return button;
			}
		}
		luaL_error(state, "unknown mouse button '%s', use Left, Right or Middle", name.data());
	}

	static MouseButton get(lua_State *state, int index) { return check(state, index); }
};

namespace Aquila::Scripting {

namespace {

entt::entity from_script(ScriptEntity entity) {
	return static_cast<entt::entity>(entity.id);
}

struct ScriptCamera {
	SceneManagement::Entity entity;
};

} // namespace

} // namespace Aquila::Scripting

template <> struct aquiLua::Converter<Aquila::Scripting::ScriptCamera> {
	using ScriptCamera = Aquila::Scripting::ScriptCamera;

	static constexpr const char *luau_type = "Entity?";

	static void push(lua_State *state, const ScriptCamera &camera) {
		aquiLua::push(state, Aquila::Scripting::ScriptEntity{ entt::to_integral(camera.entity.get_handle()) });
	}

	static ScriptCamera check(lua_State *state, int index) {
		auto &scene = *aquiLua::context<Aquila::Scripting::ScriptEnvironment>(state).scene;
		if (lua_isnoneornil(state, index)) {
			if (!scene.has_active_camera()) {
				luaL_error(state, "the scene has no active camera");
			}
			return { scene.get_active_camera_entity() };
		}
		const auto handle = aquiLua::check<Aquila::Scripting::ScriptEntity>(state, index);
		const Aquila::SceneManagement::Entity entity(Aquila::Scripting::from_script(handle), &scene);
		if (!entity.is_valid()) {
			luaL_error(state, "entity %u was destroyed", handle.id);
		}
		if (!entity.has_all_components<Aquila::SceneManagement::Components::CameraComponent,
									   Aquila::SceneManagement::Components::TransformComponent>()) {
			luaL_error(state, "entity %u has no Camera component", handle.id);
		}
		return { entity };
	}

	static ScriptCamera get(lua_State *state, int index) { return check(state, index); }
};

namespace Aquila::Scripting {

namespace {

using Platform::Events::KeyCode;
using Platform::Events::MouseButton;
using SceneManagement::Components::CameraComponent;
using SceneManagement::Components::MetadataComponent;
using SceneManagement::Components::SceneNodeComponent;
using SceneManagement::Components::TransformComponent;

ScriptEntity to_script(entt::entity entity) {
	return ScriptEntity{ entt::to_integral(entity) };
}

Vec3 flat(Vec2 value) {
	return { value.x, value.y, 0.F };
}

void log(const std::string &message) {
	AQUILA_LOG_INFO("[Luau] {}", message);
}

void warn(const std::string &message) {
	AQUILA_LOG_WARNING("[Luau] {}", message);
}

void error(const std::string &message) {
	AQUILA_LOG_ERROR("[Luau] {}", message);
}

void request_frame() {
	Foundation::FrameScheduler::get()->request_frame();
}

bool is_key_down(ScriptEnvironment &environment, KeyCode key) {
	return environment.input != nullptr && environment.input->is_key_down(key);
}

bool is_key_just_pressed(ScriptEnvironment &environment, KeyCode key) {
	return environment.input != nullptr && environment.input->is_key_just_pressed(key);
}

bool is_key_just_released(ScriptEnvironment &environment, KeyCode key) {
	return environment.input != nullptr && environment.input->is_key_just_released(key);
}

bool is_mouse_button_down(ScriptEnvironment &environment, MouseButton button) {
	return environment.input != nullptr && environment.input->is_mouse_button_down(button);
}

bool is_mouse_button_just_pressed(ScriptEnvironment &environment, MouseButton button) {
	return environment.input != nullptr && environment.input->is_mouse_button_just_pressed(button);
}

bool is_mouse_button_just_released(ScriptEnvironment &environment, MouseButton button) {
	return environment.input != nullptr && environment.input->is_mouse_button_just_released(button);
}

Vec3 mouse_position(ScriptEnvironment &environment) {
	return environment.input != nullptr ? flat(environment.input->get_mouse_position()) : Vec3(0.F);
}

Vec3 mouse_motion(ScriptEnvironment &environment) {
	return environment.input != nullptr ? flat(environment.input->get_mouse_motion()) : Vec3(0.F);
}

Vec3 scroll(ScriptEnvironment &environment) {
	return environment.input != nullptr ? flat(environment.input->get_scroll()) : Vec3(0.F);
}

Vec3 screen_size(ScriptEnvironment &environment) {
	return environment.input != nullptr ? flat(environment.input->get_view_size()) : Vec3(0.F);
}

F32 delta(ScriptEnvironment &environment) {
	return environment.delta_time;
}

F64 elapsed(ScriptEnvironment &environment) {
	return environment.elapsed;
}

Uint64 frame(ScriptEnvironment &environment) {
	return environment.frame;
}

Option<ScriptEntity> find(ScriptEnvironment &environment, const std::string &name) {
	if (const auto entity = environment.scene->get_entity_manager()->find_entity_by_name(name)) {
		return to_script(entity->get_handle());
	}
	return std::nullopt;
}

ScriptEntity create(ScriptEnvironment &environment, const std::string &name) {
	return to_script(environment.scene->get_entity_manager()->create_entity(name).get_handle());
}

void destroy(ScriptEnvironment &environment, ScriptEntity entity) {
	const SceneManagement::Entity target(from_script(entity), environment.scene);
	if (target.is_valid()) {
		environment.scene->get_entity_manager()->queue_for_kill(target);
	}
}

CameraComponent &lens(ScriptCamera &camera) {
	return camera.entity.get_component<CameraComponent>();
}

Rendering::RenderView view_of(ScriptCamera &camera) {
	return Rendering::render_view_from_entity(lens(camera), camera.entity.get_component<TransformComponent>());
}

Rect screen_rect(const ScriptEnvironment &environment) {
	const Vec2 size = environment.input != nullptr ? environment.input->get_view_size() : Vec2(0.F);
	return { .position = Vec2(0.F), .size = Vec2(Math::max(size.x, 1.F), Math::max(size.y, 1.F)) };
}

Option<ScriptEntity> active_camera(ScriptEnvironment &environment) {
	if (!environment.scene->has_active_camera()) {
		return std::nullopt;
	}
	return to_script(environment.scene->get_active_camera_entity().get_handle());
}

void set_active_camera(ScriptEnvironment &environment, ScriptCamera camera) {
	environment.scene->set_active_camera(camera.entity);
}

F32 camera_fov(ScriptEnvironment &, ScriptCamera camera) {
	return lens(camera).fov;
}

void set_camera_fov(ScriptEnvironment &, F32 degrees, ScriptCamera camera) {
	lens(camera).set_fov(degrees);
}

F32 camera_near(ScriptEnvironment &, ScriptCamera camera) {
	return lens(camera).near_plane;
}

void set_camera_near(ScriptEnvironment &, F32 distance, ScriptCamera camera) {
	CameraComponent &camera_lens = lens(camera);
	camera_lens.near_plane = Math::clamp(distance, 0.001F, camera_lens.far_plane * 0.999F);
}

F32 camera_far(ScriptEnvironment &, ScriptCamera camera) {
	return lens(camera).far_plane;
}

void set_camera_far(ScriptEnvironment &, F32 distance, ScriptCamera camera) {
	CameraComponent &camera_lens = lens(camera);
	camera_lens.far_plane = Math::max(distance, camera_lens.near_plane * 1.001F);
}

Option<Vec3> world_to_screen(ScriptEnvironment &environment, Vec3 point, ScriptCamera camera) {
	if (environment.input == nullptr || environment.input->get_view_size().x <= 0.F) {
		return std::nullopt;
	}
	if (const Option<Vec2> screen = Rendering::project_to_screen(view_of(camera), screen_rect(environment), point)) {
		return Vec3(*screen, 0.F);
	}
	return std::nullopt;
}

Vec3 screen_ray_origin(ScriptEnvironment &environment, Vec3 point, ScriptCamera camera) {
	return Rendering::screen_ray(view_of(camera), screen_rect(environment), Vec2(point)).origin;
}

Vec3 screen_ray_direction(ScriptEnvironment &environment, Vec3 point, ScriptCamera camera) {
	return Rendering::screen_ray(view_of(camera), screen_rect(environment), Vec2(point)).direction;
}

SceneManagement::Entity check_entity(lua_State *state) {
	const auto handle = aquiLua::check<ScriptEntity>(state, 1);
	const SceneManagement::Entity entity(from_script(handle), &aquiLua::context<SceneManagement::Scene>(state));
	if (!entity.is_valid()) {
		luaL_error(state, "entity %u was destroyed", handle.id);
	}
	return entity;
}

template <typename T> T *check_component(lua_State *state) {
	T *component = check_entity(state).try_get_component<T>();
	if (component == nullptr) {
		luaL_error(state, "entity %u is missing a component this method needs",
				   aquiLua::check<ScriptEntity>(state, 1).id);
	}
	return component;
}

template <typename Self> decltype(auto) resolve_self(lua_State *state) {
	if constexpr (std::is_same_v<Self, SceneManagement::Entity>) {
		return check_entity(state);
	} else {
		return *check_component<Self>(state);
	}
}

template <typename Self, typename R, typename... Args, Usize... I>
int call_free_method(lua_State *state, R (*fn)(Self, Args...), std::index_sequence<I...>) {
	decltype(auto) self = resolve_self<std::remove_cvref_t<Self>>(state);
	return aquiLua::push_result(
		state, [&] { return fn(self, aquiLua::check<std::remove_cvref_t<Args>>(state, static_cast<int>(I) + 2)...); });
}

template <typename Self, typename R, typename... Args> int call_free_method(lua_State *state, R (*fn)(Self, Args...)) {
	return call_free_method(state, fn, std::index_sequence_for<Args...>{});
}

template <typename Self, typename R, typename... Args>
aquiLua::LuauSignature free_method_signature(R (*)(Self, Args...)) {
	return aquiLua::make_signature<R, Args...>();
}

template <auto Fn> aquiLua::Method free_method(const char *name, const char *doc) {
	return { name, [](lua_State *state) { return call_free_method(state, Fn); },
			 [] { return free_method_signature(Fn); }, doc };
}

void set_name(SceneManagement::Entity entity, const std::string &name) {
	entity.patch_component<MetadataComponent>([&name](MetadataComponent &metadata) { metadata.set_name(name); });
}

void set_visible(SceneManagement::Entity entity, bool visible) {
	entity.patch_component<MetadataComponent>(
		[visible](MetadataComponent &metadata) { metadata.set_visible(visible); });
}

Vec3 forward(TransformComponent &transform) {
	return Math::normalize(transform.get_world_rotation() * Vec3(0.F, 0.F, 1.F));
}

Option<ScriptEntity> parent(SceneNodeComponent &node) {
	return node.parent.is_null() ? std::nullopt : Option<ScriptEntity>(to_script(node.parent.get_handle()));
}

int entity_id(lua_State *state) {
	aquiLua::push(state, aquiLua::check<ScriptEntity>(state, 1).id);
	return 1;
}

int entity_is_valid(lua_State *state) {
	const ScriptEntity handle = aquiLua::check<ScriptEntity>(state, 1);
	aquiLua::push(state, aquiLua::context<SceneManagement::Scene>(state).get_registry().valid(from_script(handle)));
	return 1;
}

aquiLua::LuauSignature returns_number() {
	return { .params = {}, .result = "number" };
}

aquiLua::LuauSignature returns_boolean() {
	return { .params = {}, .result = "boolean" };
}

} // namespace

void register_engine_api(aquiLua::Runtime &runtime, ScriptEnvironment &environment) {
	runtime.bind<log>("native.log", "Writes message to the engine log.");
	runtime.bind<warn>("native.warn", "Writes message to the engine log as a warning.");
	runtime.bind<error>("native.error", "Writes message to the engine log as an error.");
	runtime.bind<request_frame>("native.request_frame", "Requests another frame.");

	runtime.bind<is_key_down>("input.is_key_down", &environment, "Held right now.");
	runtime.bind<is_key_just_pressed>("input.is_key_just_pressed", &environment, "Went down this frame.");
	runtime.bind<is_key_just_released>("input.is_key_just_released", &environment, "Went up this frame.");
	runtime.bind<is_mouse_button_down>("input.is_mouse_button_down", &environment, "Held right now.");
	runtime.bind<is_mouse_button_just_pressed>("input.is_mouse_button_just_pressed", &environment,
											   "Went down this frame.");
	runtime.bind<is_mouse_button_just_released>("input.is_mouse_button_just_released", &environment,
												"Went up this frame.");
	runtime.bind<mouse_position>("input.get_mouse_position", &environment, "Pixels from the game view's corner.");
	runtime.bind<mouse_motion>("input.get_mouse_motion", &environment, "Pixels moved this frame.");
	runtime.bind<scroll>("input.get_scroll", &environment, "Wheel steps this frame.");

	runtime.bind<delta>("time.get_delta", &environment, "Seconds the frame took.");
	runtime.bind<elapsed>("time.get_elapsed", &environment, "Seconds played.");
	runtime.bind<frame>("time.get_frame", &environment, "Frames played.");

	runtime.bind<find>("scene.find", &environment, "The first entity with that name.");
	runtime.bind<create>("scene.create", &environment, "A new entity with a transform.");
	runtime.bind<destroy>("scene.destroy", &environment, "Destroys it at the end of the frame.");


	runtime.bind<screen_size>("screen.get_size", &environment, "Pixels the game is shown in.");

	runtime.bind<active_camera>("camera.get_active", &environment, "The camera the game renders through.");
	runtime.bind<set_active_camera>("camera.set_active", &environment, "Renders through this camera.");
	runtime.bind<camera_fov>("camera.get_fov", &environment, "Vertical field of view in degrees.");
	runtime.bind<set_camera_fov>("camera.set_fov", &environment, "Vertical field of view in degrees.");
	runtime.bind<camera_near>("camera.get_near", &environment, "Near clip distance.");
	runtime.bind<set_camera_near>("camera.set_near", &environment, "Near clip distance.");
	runtime.bind<camera_far>("camera.get_far", &environment, "Far clip distance.");
	runtime.bind<set_camera_far>("camera.set_far", &environment, "Far clip distance.");
	runtime.bind<world_to_screen>("camera.world_to_screen", &environment, "World point to screen pixels.");
	runtime.bind<screen_ray_origin>("camera.screen_ray_origin", &environment, "Where the ray under a pixel starts.");
	runtime.bind<screen_ray_direction>("camera.screen_ray_direction", &environment,
									   "Where the ray under a pixel goes.");

	runtime.add_alias("aquila", Foundation::EnginePaths::resources_dir() + "Engine/API");

	runtime.register_type<ScriptEntity>(
		{
			aquiLua::Method("get_id", entity_id, returns_number,
							"A number that stays the same for this entity, to compare entities or key tables."),
			aquiLua::Method("is_valid", entity_is_valid, returns_boolean,
							"False once it's destroyed. Every other method errors then."),
			aquiLua::make_method<check_component<MetadataComponent>, &MetadataComponent::get_name>(
				"get_name", "Returns the name shown in the editor."),
			free_method<set_name>("set_name", "Renames the entity."),
			aquiLua::make_method<check_component<MetadataComponent>, &MetadataComponent::is_visible>(
				"is_visible", "Hidden entities aren't drawn."),
			free_method<set_visible>("set_visible", "Shows or hides the entity."),
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
			aquiLua::make_method<check_component<TransformComponent>, &TransformComponent::get_world_position>(
				"get_world_position",
				"Position in the world, as of the last frame (moves made this frame show "
				"up next frame)."),
			free_method<forward>("get_forward", "The direction its +Z axis points in the world, as of the last frame."),
			free_method<parent>("get_parent", "Its parent, nil at the root."),
		},
		environment.scene, "An entity in the scene. A script gets the entity it is attached to as self.");
}

} // namespace Aquila::Scripting
