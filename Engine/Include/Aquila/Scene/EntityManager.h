#ifndef AQUILA_ENTITY_MNG_H
#define AQUILA_ENTITY_MNG_H

#include "Aquila/Scene/Entity.h"
#include "Aquila/Scene/Scene.h"

namespace Aquila::SceneManagement {

class EntityManager {
  public:
	explicit EntityManager(Scene *scene) : m_scene(scene) { m_registry = {}; }
	~EntityManager();

	entt::registry &get_registry();

	/// A bare entity, no components. create_entity() is the one with name, transform and scene node.
	static Entity create(Scene *scene) {
		const auto handle = scene->get_registry().create();
		return Entity(handle, scene);
	}

	static std::vector<Entity> create_many(Scene *scene, size_t count) {
		std::vector<Entity> entities;
		entities.reserve(count);

		auto handles = std::vector<entt::entity>(count);
		scene->get_registry().create(handles.begin(), handles.end());

		for (auto handle : handles) {
			entities.emplace_back(handle, scene);
		}
		return entities;
	}

	/// With Metadata (new UUID, unique name), SceneNode and Transform.
	Entity create_entity(const std::string &name);
	void apply_preset(Entity &entity, EntityPreset preset);

	/// `func` can take (Entity, Components &...), (Components &...) or just (Entity).
	///
	/// @code
	/// manager.for_each<TransformComponent, LightComponent>([](TransformComponent &transform, LightComponent &light) {
	///     light.set_direction(transform.get_world_rotation() * Vec3(0.F, 0.F, 1.F));
	/// });
	/// @endcode
	template <typename... Components, typename Func> void for_each(Func &&func) {
		AQUILA_ASSERT(m_scene, "There should be an active scene");
		for (auto view = m_registry.view<Components...>(); auto entity_handle : view) {
			Entity entity(entity_handle, m_scene);
			if constexpr (std::is_invocable_v<Func, Entity, Components &...>) {
				func(entity, view.template get<Components>(entity_handle)...);
			} else if constexpr (std::is_invocable_v<Func, Components &...>) {
				func(view.template get<Components>(entity_handle)...);
			} else {
				func(entity);
			}
		}
	}

	template <typename... Components, typename Func> void for_each(Func &&func) const {
		AQUILA_ASSERT(m_scene, "There should be an active scene");
		for (auto view = m_registry.view<const Components...>(); auto entity_handle : view) {
			Entity entity(entity_handle, const_cast<Scene *>(m_scene));
			if constexpr (std::is_invocable_v<Func, Entity, const Components &...>) {
				func(entity, view.template get<const Components>(entity_handle)...);
			} else if constexpr (std::is_invocable_v<Func, const Components &...>) {
				func(view.template get<const Components>(entity_handle)...);
			} else {
				func(entity);
			}
		}
	}

	template <typename... Components> [[nodiscard]] size_t count() const {
		return m_registry.view<Components...>().size();
	}

	template <typename... Components> Entity get_first_entity_with() {
		AQUILA_ASSERT(m_scene, "There should be an active scene");
		auto view = m_registry.view<Components...>();

		if (view.begin() == view.end()) {
			return Entity{};
		}

		return Entity{ *view.begin(), m_scene };
	}

	template <typename... Components> std::vector<Entity> get_all_with() {
		AQUILA_ASSERT(m_scene, "There should be an active scene");
		auto view = m_registry.view<Components...>();

		std::vector<Entity> result{};
		result.reserve(std::distance(view.begin(), view.end()));

		for (auto entity : view) {
			result.emplace_back(Entity{ entity, m_scene });
		}

		return result;
	}

	template <typename... Components, typename Predicate> Entity find_first(Predicate &&predicate) {
		AQUILA_ASSERT(m_scene, "There should be an active scene");
		auto view = m_registry.view<Components...>();
		for (auto entity_handle : view) {
			Entity entity(entity_handle, m_scene);
			if (predicate(entity, view.template get<Components>(entity_handle)...)) {
				return entity;
			}
		}
		return Entity::null();
	}

	template <typename... Components> auto get_view() { return m_registry.view<Components...>(); }

	template <typename... Owned, typename... Get, typename... Exclude> auto get_group() {
		return m_registry.group<Owned...>(entt::get<Get...>, entt::exclude<Exclude...>);
	}

	std::optional<Entity> find_entity_by_name(const std::string &name);
	std::optional<Entity> find_entity_by_uuid(const Foundation::UUID &uuid);
	std::string get_default_name(EntityPreset preset);
	[[nodiscard]] std::vector<Entity> get_children(Entity parent) const;
	[[nodiscard]] std::optional<Entity> get_parent(Entity child) const;

	[[nodiscard]] bool exists(const Foundation::UUID &uuid);
	[[nodiscard]] bool is_valid(Entity entity) const;
	[[nodiscard]] bool is_registry_empty();
	/// Children too.
	void destroy_entity(Entity entity);
	/// Destroyed on the next flush_deletion_queue(), safe while iterating.
	void queue_for_kill(Entity entity);
	void flush_deletion_queue();

	void construct_scene_graph();
	/// Keeps the child's local transform, so it moves with the new parent.
	void add_child(Entity parent, Entity child);
	/// Keeps the node's world transform, so it stays where it is on screen.
	void attach_to(Entity parent, Entity node);
	void remove_child(Entity parent, Entity child);
	/// Destroys the children, it doesn't just detach them.
	void remove_all_children(Entity parent);
	/// True when `entity_to_check` is somewhere under `potential_parent`.
	bool is_descendant(Entity potential_parent, Entity entity_to_check);
	std::string generate_unique_name(const std::string &base_name);

	void clear();

  private:
	void on_scene_node_construct(entt::registry &registry, entt::entity entity_handle);
	void on_scene_node_destroy(entt::registry &registry, entt::entity entity_handle);

	Scene *m_scene;
	entt::registry m_registry;
	std::vector<entt::entity> m_deletion_queue;
};

} // namespace Aquila::SceneManagement

#endif
