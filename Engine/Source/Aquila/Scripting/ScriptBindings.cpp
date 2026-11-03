#include "Aquila/Scripting/ScriptBindings.h"
#include "Aquila/Foundation/EnginePaths.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Platform/Events/KeyChord.h"
#include "Aquila/Platform/InputState.h"
#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/Rendering/ViewMath.h"
#include "Aquila/Scene/ComponentRegistry.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Scene/Components/SceneNodeComponent.h"
#include "Aquila/Scene/Components/ScriptComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scene/Visibility.h"

template <> struct aquiLua::Converter<Aquila::Platform::Events::KeyCode> {
	using KeyCode = Aquila::Platform::Events::KeyCode;

	static constexpr const char *luau_type = "string";

	static Option<KeyCode> from_name(std::string_view name) {
		constexpr std::array<std::pair<std::string_view, KeyCode>, 6> K_SIDES{ {
			{ "LeftShift", KeyCode::LeftShift },
			{ "RightShift", KeyCode::RightShift },
			{ "LeftCtrl", KeyCode::LeftControl },
			{ "RightCtrl", KeyCode::RightControl },
			{ "LeftAlt", KeyCode::LeftAlt },
			{ "RightAlt", KeyCode::RightAlt },
		} };
		for (const auto &[side, key] : K_SIDES) {
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
	static constexpr std::array<std::pair<std::string_view, MouseButton>, 3> K_NAMES{ {
		{ "Left", MouseButton::Left },
		{ "Right", MouseButton::Right },
		{ "Middle", MouseButton::Middle },
	} };

	static void push(lua_State *state, MouseButton button) {
		for (const auto &[name, value] : K_NAMES) {
			if (value == button) {
				aquiLua::push(state, name);
				return;
			}
		}
		lua_pushnil(state);
	}

	static MouseButton check(lua_State *state, int index) {
		const std::string_view name = luaL_checkstring(state, index);
		for (const auto &[button_name, button] : K_NAMES) {
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

struct RaycastHit {
	ScriptEntity entity;
	Vec3 position;
	Vec3 normal;
	F32 distance = 0.F;
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

template <> struct aquiLua::Converter<Option<Aquila::Scripting::RaycastHit>> {
	using RaycastHit = Aquila::Scripting::RaycastHit;

	static constexpr const char *luau_type = "{ entity: Entity, position: vector, normal: vector, distance: number }?";

	static void push(lua_State *state, const Option<RaycastHit> &hit) {
		if (!hit) {
			lua_pushnil(state);
			return;
		}
		lua_createtable(state, 0, 4);
		aquiLua::push(state, hit->entity);
		lua_setfield(state, -2, "entity");
		aquiLua::push(state, hit->position);
		lua_setfield(state, -2, "position");
		aquiLua::push(state, hit->normal);
		lua_setfield(state, -2, "normal");
		aquiLua::push(state, hit->distance);
		lua_setfield(state, -2, "distance");
	}

	static Option<RaycastHit> check(lua_State *state, int) { luaL_error(state, "raycast hits only go to scripts"); }

	static Option<RaycastHit> get(lua_State *, int) { return std::nullopt; }
};

namespace Aquila::Scripting {

namespace {

using Platform::Events::KeyCode;
using Platform::Events::MouseButton;
using SceneManagement::Components::CameraComponent;
using SceneManagement::Components::EntityReference;
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

ScriptEntity create(ScriptEnvironment &environment, const std::string &name) {
	return to_script(environment.scene->get_entity_manager()->create_entity(name).get_handle());
}

Option<ScriptEntity> find_by_uuid(ScriptEnvironment &environment, const std::string &uuid) {
	const Foundation::UUID id = Foundation::UUID::from_string(uuid);
	if (id == Foundation::UUID::null()) {
		return std::nullopt;
	}
	if (const auto entity = environment.scene->get_entity_manager()->find_entity_by_uuid(id)) {
		return to_script(entity->get_handle());
	}
	return std::nullopt;
}

void declare(ScriptEnvironment &environment, ScriptField field) {
	if (environment.loading == nullptr || environment.loading->declared == nullptr) {
		return;
	}
	std::vector<ScriptField> &declared = *environment.loading->declared;
	if (std::ranges::none_of(declared, [&field](const ScriptField &other) { return other.name == field.name; })) {
		declared.push_back(std::move(field));
	}
}

template <typename T> T value_or(const ScriptEnvironment &environment, const std::string &name, T fallback) {
	if (environment.loading == nullptr || environment.loading->values == nullptr) {
		return fallback;
	}
	const auto &values = *environment.loading->values;
	if (const auto it = values.find(name); it != values.end()) {
		if (const T *value = std::get_if<T>(&it->second)) {
			return *value;
		}
	}
	return fallback;
}

F64 number_field(ScriptEnvironment &environment, const std::string &name, F64 fallback, F64 min, F64 max, F64 step,
				 bool slider, bool integer) {
	declare(environment, { .name = name,
						   .type = ScriptFieldType::Number,
						   .default_value = fallback,
						   .min = min,
						   .max = max,
						   .step = step,
						   .slider = slider,
						   .integer = integer });
	F64 value = value_or(environment, name, fallback);
	if (min <= max) {
		value = std::clamp(value, min, max);
	}
	return integer ? std::round(value) : value;
}

bool boolean_field(ScriptEnvironment &environment, const std::string &name, bool fallback) {
	declare(environment, { .name = name, .type = ScriptFieldType::Boolean, .default_value = fallback });
	return value_or(environment, name, fallback);
}

std::string string_field(ScriptEnvironment &environment, const std::string &name, const std::string &fallback) {
	declare(environment, { .name = name, .type = ScriptFieldType::String, .default_value = fallback });
	return value_or(environment, name, fallback);
}

Vec3 vector_field(ScriptEnvironment &environment, const std::string &name, Vec3 fallback, bool color) {
	declare(environment,
			{ .name = name, .type = ScriptFieldType::Vector, .default_value = fallback, .color = color });
	return value_or(environment, name, fallback);
}

Option<ScriptEntity> entity_field(ScriptEnvironment &environment, const std::string &name) {
	declare(environment, { .name = name, .type = ScriptFieldType::Entity, .default_value = EntityReference{} });
	const EntityReference reference = value_or(environment, name, EntityReference{});
	if (reference.id == Foundation::UUID::null()) {
		return std::nullopt;
	}
	if (const auto entity = environment.scene->get_entity_manager()->find_entity_by_uuid(reference.id)) {
		return to_script(entity->get_handle());
	}
	return std::nullopt;
}

void destroy(ScriptEnvironment &environment, ScriptEntity entity) {
	const SceneManagement::Entity target(from_script(entity), environment.scene);
	if (!target.is_valid()) {
		return;
	}
	if (environment.destroying) {
		environment.destroying(entity);
	}
	environment.scene->get_entity_manager()->queue_for_kill(target);
}

void quit(ScriptEnvironment &environment) {
	environment.quit_requested = true;
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

const SceneManagement::ComponentDescriptor &check_descriptor(lua_State *state) {
	const char *name = luaL_checkstring(state, 2);
	const SceneManagement::ComponentDescriptor *descriptor = SceneManagement::ComponentRegistry::instance().find(name);
	if (descriptor == nullptr) {
		luaL_error(state, "unknown component '%s'", name);
	}
	return *descriptor;
}

void *check_instance(lua_State *state, SceneManagement::Entity entity,
					 const SceneManagement::ComponentDescriptor &descriptor) {
	void *instance = descriptor.get(entity);
	if (instance == nullptr) {
		luaL_error(state, "entity %u has no %s component", aquiLua::check<ScriptEntity>(state, 1).id,
				   descriptor.get_name().c_str());
	}
	return instance;
}

const Reflection::Property &check_property(lua_State *state, const SceneManagement::ComponentDescriptor &descriptor,
										   const void *instance) {
	const std::string_view name = luaL_checkstring(state, 3);
	const Reflection::Property *hidden = nullptr;
	for (const Reflection::Property &property : descriptor.get_type().get_properties()) {
		if (property.name != name) {
			continue;
		}
		if (property.is_visible(instance)) {
			return property;
		}
		if (hidden == nullptr) {
			hidden = &property;
		}
	}
	if (hidden == nullptr) {
		luaL_error(state, "%s has no property '%s'", descriptor.get_name().c_str(), name.data());
	}
	return *hidden;
}

void push_property_value(lua_State *state, const Reflection::Property &property,
						 const Reflection::PropertyValue &value) {
	if (property.kind == Reflection::PropertyKind::Enum) {
		const Int32 raw = std::get<Int32>(value);
		const auto option = std::ranges::find(property.options, raw, &Reflection::EnumOption::value);
		if (option != property.options.end()) {
			aquiLua::push(state, option->name);
		} else {
			aquiLua::push(state, raw);
		}
		return;
	}
	std::visit(
		[state](const auto &stored) {
			if constexpr (std::is_same_v<std::decay_t<decltype(stored)>, Vec2>) {
				aquiLua::push(state, Vec3(stored, 0.F));
			} else {
				aquiLua::push(state, stored);
			}
		},
		value);
}

Reflection::PropertyValue check_property_value(lua_State *state, const SceneManagement::ComponentDescriptor &descriptor,
											   const Reflection::Property &property) {
	constexpr int K_VALUE = 4;
	using Reflection::PropertyKind;
	const auto number = [&](F64 low, F64 high) {
		return std::clamp(luaL_checknumber(state, K_VALUE), std::max(low, static_cast<F64>(property.hints.min)),
						  std::min(high, static_cast<F64>(property.hints.max)));
	};
	switch (property.kind) {
	case PropertyKind::Bool:
		return luaL_checkboolean(state, K_VALUE) != 0;
	case PropertyKind::Int:
		return static_cast<Int32>(
			std::lround(number(std::numeric_limits<Int32>::min(), std::numeric_limits<Int32>::max())));
	case PropertyKind::Float:
		return static_cast<F32>(number(std::numeric_limits<F64>::lowest(), std::numeric_limits<F64>::max()));
	case PropertyKind::Vec2:
		return Vec2(aquiLua::check<Vec3>(state, K_VALUE));
	case PropertyKind::Vec3:
		return aquiLua::check<Vec3>(state, K_VALUE);
	case PropertyKind::Vec4:
		return aquiLua::check<Vec4>(state, K_VALUE);
	case PropertyKind::String:
		return std::string(luaL_checkstring(state, K_VALUE));
	case PropertyKind::Enum:
		break;
	}
	if (lua_type(state, K_VALUE) == LUA_TNUMBER) {
		return static_cast<Int32>(std::lround(lua_tonumber(state, K_VALUE)));
	}
	const std::string_view name = luaL_checkstring(state, K_VALUE);
	const auto option = std::ranges::find(property.options, name, &Reflection::EnumOption::name);
	if (option == property.options.end()) {
		luaL_error(state, "%s %s has no option '%s'", descriptor.get_name().c_str(), property.name.c_str(),
				   name.data());
	}
	return option->value;
}

int entity_has(lua_State *state) {
	const SceneManagement::Entity entity = check_entity(state);
	aquiLua::push(state, check_descriptor(state).has(entity));
	return 1;
}

int entity_get(lua_State *state) {
	const SceneManagement::Entity entity = check_entity(state);
	const SceneManagement::ComponentDescriptor &descriptor = check_descriptor(state);
	const void *instance = check_instance(state, entity, descriptor);
	const Reflection::Property &property = check_property(state, descriptor, instance);
	push_property_value(state, property, property.get(instance));
	return 1;
}

int entity_set(lua_State *state) {
	const SceneManagement::Entity entity = check_entity(state);
	const SceneManagement::ComponentDescriptor &descriptor = check_descriptor(state);
	void *instance = check_instance(state, entity, descriptor);
	const Reflection::Property &property = check_property(state, descriptor, instance);
	if (!property.set || property.hints.read_only) {
		luaL_error(state, "%s %s is read-only", descriptor.get_name().c_str(), property.name.c_str());
	}
	property.set(instance, check_property_value(state, descriptor, property));
	descriptor.notify_changed(entity);
	return 0;
}

int entity_add(lua_State *state) {
	const SceneManagement::Entity entity = check_entity(state);
	const SceneManagement::ComponentDescriptor &descriptor = check_descriptor(state);
	if (!descriptor.can_add() || !descriptor.is_removable()) {
		luaL_error(state, "%s can't be added", descriptor.get_name().c_str());
	}
	descriptor.add(entity);
	return 0;
}

int entity_remove(lua_State *state) {
	const SceneManagement::Entity entity = check_entity(state);
	const SceneManagement::ComponentDescriptor &descriptor = check_descriptor(state);
	if (!descriptor.is_removable()) {
		luaL_error(state, "%s can't be removed", descriptor.get_name().c_str());
	}
	descriptor.remove(entity);
	return 0;
}

aquiLua::LuauSignature component_signature() {
	return { .params = { "string" }, .result = "()" };
}

aquiLua::LuauSignature has_signature() {
	return { .params = { "string" }, .result = "boolean" };
}

aquiLua::LuauSignature get_signature() {
	return { .params = { "string", "string" }, .result = "any" };
}

aquiLua::LuauSignature set_signature() {
	return { .params = { "string", "string", "boolean | number | string | vector" }, .result = "()" };
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

Mat4 world_matrix(SceneManagement::Entity entity) {
	Mat4 world(1.F);
	for (SceneManagement::Entity at = entity; at.is_valid();) {
		if (const auto *transform = at.try_get_component<TransformComponent>()) {
			world = transform->get_local_transform_matrix() * world;
		}
		const auto *node = at.try_get_component<SceneNodeComponent>();
		at = node != nullptr ? node->parent : SceneManagement::Entity::null();
	}
	return world;
}

Mat4 parent_world_matrix(SceneManagement::Entity entity) {
	const auto *node = entity.try_get_component<SceneNodeComponent>();
	return node != nullptr && node->parent.is_valid() ? world_matrix(node->parent) : Mat4(1.F);
}

Quaternion rotation_of(const Mat4 &matrix) {
	return glm::quat_cast(
		Mat3(Math::normalize(Vec3(matrix[0])), Math::normalize(Vec3(matrix[1])), Math::normalize(Vec3(matrix[2]))));
}

void set_world_rotation(SceneManagement::Entity entity, const Quaternion &rotation) {
	if (auto *transform = entity.try_get_component<TransformComponent>()) {
		transform->set_local_rotation(
			Math::normalize(Math::inverse(rotation_of(parent_world_matrix(entity))) * rotation));
	}
}

Vec3 world_position(SceneManagement::Entity entity) {
	return Vec3(world_matrix(entity)[3]);
}

void set_world_position(SceneManagement::Entity entity, Vec3 position) {
	if (auto *transform = entity.try_get_component<TransformComponent>()) {
		transform->set_local_position(Vec3(Math::inverse(parent_world_matrix(entity)) * Vec4(position, 1.F)));
	}
}

Vec3 forward(SceneManagement::Entity entity) {
	return Math::normalize(Vec3(world_matrix(entity)[2]));
}

Vec3 up(SceneManagement::Entity entity) {
	return Math::normalize(Vec3(world_matrix(entity)[1]));
}

Vec3 right(SceneManagement::Entity entity) {
	return Math::normalize(Vec3(world_matrix(entity)[0]));
}

void look_at(SceneManagement::Entity entity, Vec3 target, Option<Vec3> up) {
	const Vec3 direction = target - world_position(entity);
	if (Math::length(direction) < 1e-6F) {
		return;
	}
	const Vec3 ahead = Math::normalize(direction);
	Vec3 above = up.value_or(Vec3(0.F, 1.F, 0.F));
	if (Math::length(Math::cross(above, ahead)) < 1e-4F) {
		above = Math::abs(ahead.z) < 0.9F ? Vec3(0.F, 0.F, 1.F) : Vec3(1.F, 0.F, 0.F);
	}
	const Vec3 side = Math::normalize(Math::cross(above, ahead));
	set_world_rotation(entity, glm::quat_cast(Mat3(side, Math::cross(ahead, side), ahead)));
}

void rotate(SceneManagement::Entity entity, Vec3 axis, F32 degrees) {
	if (Math::length(axis) < 1e-6F) {
		return;
	}
	const Quaternion turn = Math::angle_axis(Math::radians(degrees), Math::normalize(axis));
	set_world_rotation(entity, turn * rotation_of(world_matrix(entity)));
}

SceneManagement::Entity clone_entity(SceneManagement::Entity source, SceneManagement::Entity parent) {
	SceneManagement::EntityManager &entities = *source.get_scene()->get_entity_manager();
	SceneManagement::Entity copy = entities.create_entity(source.get_name());
	SceneManagement::ComponentRegistry::instance().copy_components(source, copy,
																   { entt::type_hash<MetadataComponent>::value(),
																	 entt::type_hash<SceneNodeComponent>::value(),
																	 entt::type_hash<TransformComponent>::value() });
	copy.get_component<MetadataComponent>().set_visible(source.get_component<MetadataComponent>().is_visible());
	if (const auto *from = source.try_get_component<TransformComponent>()) {
		auto &to = copy.get_component<TransformComponent>();
		to.set_local_position(from->get_local_position());
		to.set_local_rotation(from->get_local_rotation());
		to.set_local_scale(from->get_local_scale());
	}
	if (parent.is_valid()) {
		entities.add_child(parent, copy);
	}
	const std::vector<SceneManagement::Entity> children = source.get_component<SceneNodeComponent>().children;
	for (const SceneManagement::Entity &child : children) {
		if (child.is_valid()) {
			clone_entity(child, copy);
		}
	}
	return copy;
}

Option<F32> box_entry(Vec3 origin, Vec3 direction, Vec3 low, Vec3 high, Vec3 &normal) {
	F32 enter = -std::numeric_limits<F32>::infinity();
	F32 exit = std::numeric_limits<F32>::infinity();
	for (int axis = 0; axis < 3; ++axis) {
		if (Math::abs(direction[axis]) < 1e-9F) {
			if (origin[axis] < low[axis] || origin[axis] > high[axis]) {
				return std::nullopt;
			}
			continue;
		}
		F32 near = (low[axis] - origin[axis]) / direction[axis];
		F32 far = (high[axis] - origin[axis]) / direction[axis];
		F32 face = -1.F;
		if (near > far) {
			std::swap(near, far);
			face = 1.F;
		}
		if (near > enter) {
			enter = near;
			normal = Vec3(0.F);
			normal[axis] = face;
		}
		exit = Math::min(exit, far);
		if (enter > exit) {
			return std::nullopt;
		}
	}
	return enter >= 0.F ? Option<F32>(enter) : std::nullopt;
}

Option<RaycastHit> raycast(ScriptEnvironment &environment, Vec3 origin, Vec3 direction, F32 max_distance,
						   Option<ScriptEntity> ignore) {
	const F32 length = Math::length(direction);
	if (length < 1e-6F) {
		return std::nullopt;
	}
	const Vec3 heading = direction / length;
	SceneManagement::Scene &scene = *environment.scene;
	SceneManagement::EntityManager &entities = *scene.get_entity_manager();
	const auto &registry = scene.get_registry();
	const SceneManagement::Entity ignored =
		ignore ? SceneManagement::Entity(from_script(*ignore), &scene) : SceneManagement::Entity::null();

	Option<RaycastHit> nearest;
	F32 best = max_distance;
	for (auto [handle, mesh] : registry.view<SceneManagement::Components::MeshComponent>().each()) {
		if (!mesh.data || !SceneManagement::is_visible_in_hierarchy(registry, handle)) {
			continue;
		}
		const SceneManagement::Entity entity(handle, &scene);
		if (ignored.is_valid() && (entity == ignored || entities.is_descendant(ignored, entity))) {
			continue;
		}
		const Mat4 to_local = Math::inverse(world_matrix(entity));
		const Graphics::Resources::MeshSurface &surface = mesh.data->get_surface();
		Vec3 local_normal(0.F);
		const Option<F32> entry =
			box_entry(Vec3(to_local * Vec4(origin, 1.F)), Vec3(to_local * Vec4(heading, 0.F)),
					  surface.center - surface.half_size, surface.center + surface.half_size, local_normal);
		if (!entry || *entry > best) {
			continue;
		}
		best = *entry;
		nearest = RaycastHit{
			.entity = to_script(handle),
			.position = origin + heading * *entry,
			.normal = Math::normalize(Mat3(Math::transpose(to_local)) * local_normal),
			.distance = *entry,
		};
	}
	return nearest;
}

ScriptEntity clone(SceneManagement::Entity entity) {
	const auto *node = entity.try_get_component<SceneNodeComponent>();
	return to_script(
		clone_entity(entity, node != nullptr ? node->parent : SceneManagement::Entity::null()).get_handle());
}

std::string uuid(MetadataComponent &metadata) {
	return metadata.get_id().to_string();
}

Option<ScriptEntity> parent(SceneNodeComponent &node) {
	return node.parent.is_null() ? std::nullopt : Option<ScriptEntity>(to_script(node.parent.get_handle()));
}

int entity_children(lua_State *state) {
	const SceneManagement::Entity entity = check_entity(state);
	const auto *node = entity.try_get_component<SceneNodeComponent>();
	lua_createtable(state, node != nullptr ? static_cast<int>(node->children.size()) : 0, 0);
	if (node == nullptr) {
		return 1;
	}
	int index = 1;
	for (const SceneManagement::Entity &child : node->children) {
		if (child.is_valid()) {
			aquiLua::push(state, to_script(child.get_handle()));
			lua_rawseti(state, -2, index++);
		}
	}
	return 1;
}

int entity_set_parent(lua_State *state) {
	SceneManagement::Entity entity = check_entity(state);
	SceneManagement::EntityManager &entities = *entity.get_scene()->get_entity_manager();
	SceneManagement::Entity parent = SceneManagement::Entity::null();
	if (const Option<ScriptEntity> handle = aquiLua::check<Option<ScriptEntity>>(state, 2)) {
		parent = SceneManagement::Entity(from_script(*handle), entity.get_scene());
		if (!parent.is_valid()) {
			luaL_error(state, "entity %u was destroyed", handle->id);
		}
		if (parent == entity || entities.is_descendant(entity, parent)) {
			luaL_error(state, "entity %u can't go under itself or its own children", handle->id);
		}
	}
	auto *node = entity.try_get_component<SceneNodeComponent>();
	auto *transform = entity.try_get_component<TransformComponent>();
	if (node == nullptr || transform == nullptr) {
		luaL_error(state, "entity %u has no Scene Node or Transform", aquiLua::check<ScriptEntity>(state, 1).id);
	}

	const Mat4 world = world_matrix(entity);
	if (node->parent.is_valid()) {
		entities.remove_child(node->parent, entity);
	}
	if (parent.is_valid()) {
		entities.add_child(parent, entity);
	}
	const Mat4 local = Math::inverse(parent.is_valid() ? world_matrix(parent) : Mat4(1.F)) * world;
	transform->set_local_position(Vec3(local[3]));
	transform->set_local_rotation(rotation_of(local));
	transform->set_local_scale(
		Vec3(Math::length(Vec3(local[0])), Math::length(Vec3(local[1])), Math::length(Vec3(local[2]))));
	return 0;
}

aquiLua::LuauSignature children_signature() {
	return { .params = {}, .result = "{Entity}" };
}

aquiLua::LuauSignature set_parent_signature() {
	return { .params = { "Entity?" }, .result = "()" };
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

	runtime.bind<create>("scene.create", &environment, "A new entity with a transform.");
	runtime.bind<raycast>("scene.raycast", &environment, "The nearest mesh box along a ray.");
	runtime.bind<find_by_uuid>("scene.find_by_uuid", &environment, "The entity with that UUID.");
	runtime.bind<destroy>("scene.destroy", &environment, "Destroys it at the end of the frame.");

	runtime.bind<number_field>("fields.number", &environment, "Declares a number field, returns its value.");
	runtime.bind<boolean_field>("fields.boolean", &environment, "Declares a boolean field, returns its value.");
	runtime.bind<string_field>("fields.string", &environment, "Declares a string field, returns its value.");
	runtime.bind<vector_field>("fields.vector", &environment, "Declares a vector field, returns its value.");
	runtime.bind<entity_field>("fields.entity", &environment, "Declares an entity field, returns the entity.");

	runtime.bind<screen_size>("screen.get_size", &environment, "Pixels the game is shown in.");

	runtime.bind<quit>("game.quit", &environment, "Stops the game once this frame's scripts have run.");

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
			free_method<world_position>("get_world_position", "Position in the world."),
			free_method<set_world_position>("set_world_position", "Moves it to a world position, whatever its parent."),
			free_method<forward>("get_forward", "The direction its +Z axis points in the world."),
			free_method<up>("get_up", "The direction its +Y axis points in the world."),
			free_method<right>("get_right", "The direction its +X axis points in the world."),
			free_method<look_at>("look_at",
								 "Turns it so +Z points at a world position, with +Y as close to `up` as it gets "
								 "(world up when left out)."),
			free_method<rotate>("rotate",
								"Turns it around a world axis through its own origin, in degrees. Pass get_up() or "
								"get_forward() to turn around its own axes."),
			free_method<parent>("get_parent", "Its parent, nil at the root."),
			aquiLua::Method("get_children", entity_children, children_signature, "Its direct children."),
			free_method<clone>(
				"clone",
				"A copy with every component, script and child, next to it under the same parent. Hidden "
				"templates make hidden copies, set_visible(true) shows them."),
			aquiLua::Method("set_parent", entity_set_parent, set_parent_signature,
							"Moves it under another entity, or to the root with nil, staying where it is in the "
							"world. Errors when the parent is itself or one of its children."),
			free_method<uuid>("get_uuid", "The id saved in scenes. Stays the same across saves, loads and Play."),
			aquiLua::Method("has", entity_has, has_signature,
							"Whether it has the component, by the name the inspector shows. Unknown names error."),
			aquiLua::Method("get", entity_get, get_signature,
							"A component property by the names the inspector shows, like get(\"Light\", \"Range\"). "
							"Options come back as their names, colors as vectors (w is alpha). When several share a "
							"name (Light's Intensity in lx, lm or cd), it's the one the inspector shows."),
			aquiLua::Method("set", entity_set, set_signature,
							"Writes a component property like the inspector does: numbers stay in its range, options "
							"go by name. Read-only properties error."),
			aquiLua::Method("add", entity_add, component_signature,
							"Adds the component with its defaults, like Add Component. Does nothing when it's there."),
			aquiLua::Method("remove", entity_remove, component_signature,
							"Removes the component. Transform, Metadata and Scene Node can't be removed."),
		},
		environment.scene, "An entity in the scene. A script gets the entity it is attached to as self.");
}

} // namespace Aquila::Scripting
