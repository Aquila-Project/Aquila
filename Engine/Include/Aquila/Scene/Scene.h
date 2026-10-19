#ifndef AQUILA_SCENE_H
#define AQUILA_SCENE_H

#include "entt.h"
#include "json.hpp"
#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/UUID.h"
#include "Aquila/Foundation/Invalidation/DirtySet.h"
#include "Aquila/Foundation/Signal.h"
#include "Components/CameraComponent.h"

namespace Aquila::Assets {
class AssetManager; // Forward declaration
}

namespace Aquila::SceneManagement {
class Entity;
class EntityManager;

class Scene final {
  public:
	Scene();
	Scene(std::string name);

	AQUILA_NONCOPYABLE(Scene);
	AQUILA_NONMOVEABLE(Scene);

	~Scene();

	void on_start();
	void clear();

	[[nodiscard]] entt::registry &get_registry() const;
	[[nodiscard]] EntityManager *get_entity_manager() const;
	[[nodiscard]] const std::string &get_scene_name() const;
	[[nodiscard]] Entity get_active_camera_entity() const;
	[[nodiscard]] bool has_active_camera() const;
	[[nodiscard]] Entity find_primary_camera() const;
	/// The primary camera, else the first camera, else an invalid entity. What a built game looks through.
	[[nodiscard]] Entity find_game_camera() const;
	[[nodiscard]] Foundation::UUID get_handle() const;

	/// Recomputes world matrices for dirty transforms and their children, parents first.
	/// Call it once per frame before anything reads world matrices.
	void update_transform_hierarchy();
	void update_transform_recursive(Entity entity, const Mat4 &parent_world);
	/// Transform setters already call it.
	void mark_transform_dirty(entt::entity entity);

	void set_scene_name(std::string name) { m_scene_name = std::move(name); }

	/// `filepath` is a VFS path.
	bool serialize(const std::string &filepath);
	/// Replaces everything in the scene with the file's entities and makes the primary camera active.
	/// GPU materials aren't created here (no GfxContext), the Application does it from on_loaded.
	bool deserialize(const std::string &filepath);

	/// Replaces everything in this scene with a copy of `other`. Entities keep their EnTT ids, so an Entity taken
	/// from this scene before a copy_from() still points at the same entity after it, as long as `other` has it.
	/// Only component types in the ComponentRegistry come along (the editor's OutlineComponent doesn't), and
	/// meshes and materials are shared, not duplicated. The scene's handle stays, on_loaded doesn't fire.
	void copy_from(const Scene &other);

	/// Fires after deserialize() succeeded.
	Signal<void()> on_loaded;
	/// An entity was created or destroyed, or its MetadataComponent was patched (Entity::patch_component, which is
	/// how scripts rename and hide entities; plain setter calls don't fire it). Fires per entity and a lot while
	/// loading, so only mark yourself dirty in it.
	Signal<void()> on_entities_changed;

	void set_active_camera(Entity camera_entity);

  protected:
	std::string m_scene_name;
	Unique<EntityManager> m_entity_manager;

  private:
	Foundation::UUID m_scene_id;
	entt::entity m_active_camera_entity = entt::null;
	Foundation::DirtySet<entt::entity> m_dirty_transforms;

	void on_transform_construct(entt::registry &registry, entt::entity entity);
	void on_metadata_changed(entt::registry &registry, entt::entity entity);
	[[nodiscard]] bool has_dirty_ancestor(entt::entity entity) const;
	[[nodiscard]] int get_entity_depth(entt::entity entity) const;

	friend class Entity;
	friend class EntityManager;
	friend class SceneGraph;
};
} // namespace Aquila::SceneManagement

#endif
