#ifndef AQUILA_ENTITY_H
#define AQUILA_ENTITY_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Scene/Scene.h"

namespace Aquila::SceneManagement {

class EntityManager;

enum class EntityPreset {
	Empty,
	Cube,
	Sphere,
	Cylinder,
	Plane,
	PointLight,
	DirectionalLight,
	SpotLight,
	EnvLight,
	OrthographicCamera,
	PerspectiveCamera,
	SkyLight
};

/// A handle: EnTT id plus scene. Cheap to copy, it doesn't own anything.
///
/// @code
/// Entity lamp = scene.get_entity_manager()->create_entity("Lamp");
/// lamp.add_component<LightComponent>(LightComponent::Type::Point, Vec3(1.F), 10.F);
/// if (auto *light = lamp.try_get_component<LightComponent>()) {
///     light->set_range(5.F);
/// }
/// @endcode
class Entity {
  public:
	Entity() = default;
	Entity(const entt::entity handle, Scene *scene) : m_entity_handle(handle), m_scene(scene) {};

	Entity(const Entity &other) = default;
	Entity &operator=(const Entity &other) = default;
	Entity(Entity &&other) noexcept = default;
	Entity &operator=(Entity &&other) noexcept = default;
	~Entity() = default;

	static Entity null() { return {}; }

	template <typename T, typename... Args> T &add_component(Args &&...args) {
		return m_scene->get_registry().emplace<T>(m_entity_handle, std::forward<Args>(args)...);
	}

	template <typename T> T &get_component() { return m_scene->get_registry().get<T>(m_entity_handle); }

	template <typename T> const T &get_component() const {
		AQUILA_ASSERT(m_scene, "There should be an active scene");
		return m_scene->get_registry().get<T>(m_entity_handle);
	}

	template <typename T> T *try_get_component() {
		if (has_component<T>()) {
			return &get_component<T>();
		}
		return nullptr;
	}

	template <typename T> const T *try_get_component() const {
		if (has_component<T>()) {
			return &get_component<T>();
		}
		return nullptr;
	}

	template <typename T> void try_remove_component() const {
		if (has_component<T>()) {
			remove_component<T>();
		}
	}

	template <typename T> void add_or_replace_component() const {
		m_scene->get_registry().emplace_or_replace<T>(m_entity_handle);
	}

	template <typename T> void add_or_replace_component(const T &component) const {
		m_scene->get_registry().emplace_or_replace<T>(m_entity_handle, component);
	}

	template <typename T, typename... Args> void add_or_replace_component(Args &&...args) const {
		m_scene->get_registry().emplace_or_replace<T>(m_entity_handle, std::forward<Args>(args)...);
	}

	template <typename T> T &get_or_emplace() { return m_scene->get_registry().get_or_emplace<T>(m_entity_handle); }

	template <typename T, typename... Args> T &get_or_emplace(Args &&...args) {
		return m_scene->get_registry().get_or_emplace<T>(m_entity_handle, std::forward<Args>(args)...);
	}

	template <typename T> [[nodiscard]] bool has_component() const {
		AQUILA_ASSERT(m_scene, "There should be an active scene");
		return m_scene->get_registry().all_of<T>(m_entity_handle);
	}

	template <typename... Components> [[nodiscard]] bool has_all_components() const {
		AQUILA_ASSERT(m_scene, "There should be an active scene");
		return m_scene->get_registry().all_of<Components...>(m_entity_handle);
	}

	template <typename... Components> [[nodiscard]] bool has_any_component() const {
		AQUILA_ASSERT(m_scene, "There should be an active scene");
		return m_scene->get_registry().any_of<Components...>(m_entity_handle);
	}

	template <typename T> void remove_component() const {
		AQUILA_ASSERT(m_scene, "There should be an active scene");
		m_scene->get_registry().remove<T>(m_entity_handle);
	}

	template <typename... Components> void remove_components() const {
		AQUILA_ASSERT(m_scene, "There should be an active scene");
		m_scene->get_registry().remove<Components...>(m_entity_handle);
	}

	template <typename T> T &replace_component(const T &component) {
		return m_scene->get_registry().replace<T>(m_entity_handle, component);
	}

	template <typename T, typename... Args> T &replace_component(Args &&...args) {
		return m_scene->get_registry().replace<T>(m_entity_handle, std::forward<Args>(args)...);
	}

	/// Edits in place and fires the registry's update listeners.
	template <typename T, typename... Args> T &patch_component(Args &&...args) {
		return m_scene->get_registry().patch<T>(m_entity_handle, std::forward<Args>(args)...);
	}

	[[nodiscard]] entt::entity get_handle() const { return m_entity_handle; }

	[[nodiscard]] Scene *get_scene() const {
		AQUILA_ASSERT(m_scene, "There should be an active scene");
		return m_scene;
	}

	[[nodiscard]] const Foundation::UUID &get_uuid() const;
	[[nodiscard]] const std::string &get_name() const;
	void set_name(const std::string &new_name);

	/// Destroys it now. Its children are queued and go on the next flush_deletion_queue(),
	/// EntityManager::destroy_entity() takes them right away.
	void kill() const;

	/// Non-null and still alive in the registry. is_null() only checks the handle.
	[[nodiscard]] bool is_valid() const;
	[[nodiscard]] bool is_null() const;

	[[nodiscard]] bool exists() const { return (m_scene != nullptr) && m_scene->get_registry().valid(m_entity_handle); }

	[[nodiscard]] entt::registry::version_type get_version() const {
		AQUILA_ASSERT(m_scene, "There should be an active scene");
		return m_scene->get_registry().current(m_entity_handle);
	}

	bool operator==(const Entity &other) const {
		return m_entity_handle == other.m_entity_handle && m_scene == other.m_scene;
	}

	bool operator!=(const Entity &other) const { return !(*this == other); }

	bool operator<(const Entity &other) const { return m_entity_handle < other.m_entity_handle; }

	explicit operator entt::entity() const { return m_entity_handle; }

	[[nodiscard]] std::string to_string() const {
		if (is_null()) {
			return "Entity::Null";
		}
		return "Entity(" + std::to_string(static_cast<uint32_t>(m_entity_handle)) + ")";
	}

	/// Not implemented yet, it only asserts.
	void copy_from(const Entity &other) const {
		AQUILA_ASSERT(m_scene && other.m_scene, "Both entities must have valid scenes");
	}

  private:
	entt::entity m_entity_handle = entt::null;
	Scene *m_scene = nullptr;

	friend class EntityManager;
};

} // namespace Aquila::SceneManagement

namespace std {
template <> struct hash<Aquila::SceneManagement::Entity> {
	size_t operator()(const Aquila::SceneManagement::Entity &entity) const noexcept {
		return hash<entt::entity>{}(entity.get_handle());
	}
};
} // namespace std

#endif
