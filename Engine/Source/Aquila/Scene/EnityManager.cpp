#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Scene/Components/MaterialComponent.h"
#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Scene/Components/SceneNodeComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/Scene.h"

namespace Aquila::SceneManagement {

EntityManager::~EntityManager() = default;

entt::registry &EntityManager::get_registry() {
	AQUILA_ASSERT(m_scene, "Scene should not be nullptr");
	return m_registry;
}

void EntityManager::queue_for_kill(Entity entity) {
	if (!entity.is_valid()) {
		return;
	}
	m_deletion_queue.emplace_back(entity.get_handle());
}

void EntityManager::flush_deletion_queue() {
	while (!m_deletion_queue.empty()) {
		std::vector<entt::entity> pending;
		pending.swap(m_deletion_queue);
		for (const entt::entity handle : pending) {
			destroy_entity(Entity{ handle, m_scene });
		}
	}
}

bool EntityManager::is_registry_empty() {
	return m_registry.storage<entt::entity>().empty();
}

std::string EntityManager::get_default_name(EntityPreset preset) {
	switch (preset) {
	case EntityPreset::Empty:
		return "Empty Entity";
	case EntityPreset::Cube:
		return "Cube";
	case EntityPreset::Sphere:
		return "Sphere";
	case EntityPreset::Cylinder:
		return "Cylinder";
	case EntityPreset::Plane:
		return "Plane";
	case EntityPreset::DirectionalLight:
		return "Directional Light";
	case EntityPreset::PointLight:
		return "Point Light";
	case EntityPreset::SpotLight:
		return "Spot Light";
	case EntityPreset::PerspectiveCamera:
		return "Camera";
	case EntityPreset::OrthographicCamera:
		return "Orthographic Camera";
	default:
		return "Empty Entity";
	}
}

Entity EntityManager::create_entity(const std::string &name) {
	AQUILA_ASSERT(m_scene, "Scene should not be nullptr");

	Entity entity{ m_registry.create(), m_scene };

	std::string unique_name = generate_unique_name(name);

	entity.add_component<Components::MetadataComponent>(Foundation::UUID::generate(), unique_name, true);
	entity.add_component<Components::SceneNodeComponent>();
	entity.add_component<Components::TransformComponent>();

	return entity;
}

void EntityManager::destroy_entity(Entity entity) {
	if (!entity.is_valid()) {
		return;
	}

	remove_all_children(entity);

	entity.kill();
}

bool EntityManager::is_valid(Entity entity) const {
	return entity.is_valid();
}

std::string EntityManager::generate_unique_name(const std::string &base_name) {
	auto entities = get_all_with<Components::MetadataComponent>();

	std::unordered_set<std::string> existing_names;
	for (auto entity : entities) {
		auto &metadata = entity.get_component<Components::MetadataComponent>();
		existing_names.insert(metadata.get_name());
	}

	if (!existing_names.contains(base_name)) {
		return base_name;
	}

	int counter = 1;
	std::string candidate_name;
	do {
		candidate_name = base_name + " (" + std::to_string(counter) + ")";
		counter++;
	} while (existing_names.contains(candidate_name));

	return candidate_name;
}

bool EntityManager::exists(const Foundation::UUID &uuid) {
	auto entities = get_all_with<Components::MetadataComponent>();

	for (auto entity : entities) {
		auto &metadata = entity.get_component<Components::MetadataComponent>();
		if (metadata.get_id() == uuid) {
			return true;
		}
	}

	return false;
}

std::optional<Entity> EntityManager::find_entity_by_uuid(const Foundation::UUID &uuid) {
	auto entities = get_all_with<Components::MetadataComponent>();

	for (auto entity : entities) {
		auto &metadata = entity.get_component<Components::MetadataComponent>();
		if (metadata.get_id() == uuid) {
			return entity;
		}
	}

	return std::nullopt;
}

std::optional<Entity> EntityManager::find_entity_by_name(const std::string &name) {
	auto entities = get_all_with<Components::MetadataComponent>();

	for (auto entity : entities) {
		auto &metadata = entity.get_component<Components::MetadataComponent>();
		if (metadata.get_name() == name) {
			return entity;
		}
	}

	return std::nullopt;
}

void EntityManager::clear() {
	for (auto [entity] : m_registry.storage<entt::entity>().each()) {
		m_registry.destroy(entity);
	}

	m_registry.clear();
	m_deletion_queue.clear();
}

/**
 * @brief Constructs the scene graph by connecting callbacks to
 * SceneNodeComponent events.
 *
 * This function is called to set up the scene graph, allowing it to respond to
 * construction, update, and destruction of SceneNodeComponents in the entt
 * registry.
 */
void EntityManager::construct_scene_graph() {
	// Connect callbacks to SceneNodeComponent events
	m_registry.on_construct<Components::SceneNodeComponent>().connect<&EntityManager::on_scene_node_construct>(*this);
	m_registry.on_destroy<Components::SceneNodeComponent>().connect<&EntityManager::on_scene_node_destroy>(*this);
}

/**
 * @brief Constructs a scene node and sets its parent-child relationship in the
 * scene graph.
 *
 * This function is called when a SceneNodeComponent is constructed in the
 * registry. It initializes the SceneNodeComponent and establishes
 * the parent-child relationship if a parent is specified.
 *
 * @param registry The entt registry containing the scene graph.
 * @param entityHandle The entity being constructed.
 */
void EntityManager::on_scene_node_construct(entt::registry &registry, entt::entity entity_handle) {
	Entity entity(entity_handle, m_scene);

	auto &node = entity.get_or_emplace<Components::SceneNodeComponent>();
	node.ent = entity;

	if (!node.parent.is_null()) {
		auto &parent_node = m_registry.get_or_emplace<Components::SceneNodeComponent>(node.parent.get_handle());

		auto &siblings = parent_node.children;
		if (std::ranges::find(siblings, entity) == siblings.end()) {
			siblings.push_back(entity);
		}
	}
}

void EntityManager::on_scene_node_destroy(entt::registry &registry, entt::entity entity_handle) {
	Entity entity(entity_handle, m_scene);

	auto *node = entity.try_get_component<Components::SceneNodeComponent>();
	if (node == nullptr) {
		return;
	}

	for (auto child : node->children) {
		if (child.is_valid()) {
			queue_for_kill(child);
		}
	}
	node->children.clear();

	if (!node->parent.is_null()) {
		auto *parent_node = node->parent.try_get_component<Components::SceneNodeComponent>();
		if (parent_node != nullptr) {
			auto &siblings = parent_node->children;
			if (!siblings.empty()) {
				std::erase(siblings, entity);
			}
		}
	}
}

/**
 * @brief Adds a child entity to a parent in the scene graph.
 *
 * @param parent The parent entity to which the child will be added.
 * @param child The child entity to be added.
 */
void EntityManager::add_child(Entity parent, Entity child) {
	if (!parent.is_valid() || !child.is_valid()) {
		return;
	}

	auto *parent_node = parent.try_get_component<Components::SceneNodeComponent>();
	auto *child_node = child.try_get_component<Components::SceneNodeComponent>();
	if ((parent_node == nullptr) || (child_node == nullptr)) {
		return;
	}

	if (parent == child || is_descendant(child, parent)) {
		return;
	}

	if (child_node->parent.is_valid() && child_node->parent != parent) {
		if (auto *old_parent_node = child_node->parent.try_get_component<Components::SceneNodeComponent>()) {
			std::erase(old_parent_node->children, child);
		}
	}

	child_node->parent = parent_node->ent;

	if (std::ranges::find(parent_node->children, child) == parent_node->children.end()) {
		parent_node->children.push_back(child);
	}
}

/**
 * @brief Attaches a node to a parent in the scene graph.
 *
 * @param parent The parent entity to which the node will be attached.
 * @param node The node entity to be attached.
 */
void EntityManager::attach_to(Entity parent, Entity node) {
	if (!parent.is_valid() || !node.is_valid()) {
		return;
	}

	auto *parent_node = parent.try_get_component<Components::SceneNodeComponent>();
	auto *node_to_attach = node.try_get_component<Components::SceneNodeComponent>();
	if ((parent_node == nullptr) || (node_to_attach == nullptr)) {
		return;
	}

	auto *node_transform = node.try_get_component<Components::TransformComponent>();
	auto *parent_transform = parent.try_get_component<Components::TransformComponent>();
	if ((node_transform == nullptr) || (parent_transform == nullptr)) {
		return;
	}

	// Store the current world transform before reparenting
	Vec3 world_pos = node_transform->get_world_position();
	Quaternion world_rot = node_transform->get_world_rotation();
	Vec3 world_scale = node_transform->get_world_scale();

	// if has parent -> detach from parent
	if (node_to_attach->parent.get_handle() != entt::null) {
		remove_child(node_to_attach->parent, node_to_attach->ent);
	}

	// reattach to another parent
	node_to_attach->parent = parent_node->ent;
	parent_node->children.push_back(node_to_attach->ent);

	// Convert world transform to local space relative to new parent
	Mat4 parent_world = parent_transform->get_world_matrix_lazy();
	Mat4 parent_inverse = inverse(parent_world);

	// Calculate local position
	Vec4 local_pos4 = parent_inverse * Vec4(world_pos, 1.0F);
	Vec3 local_pos = Vec3(local_pos4);

	// Calculate local rotation
	Quaternion parent_world_rot = parent_transform->get_world_rotation();
	Quaternion local_rot = Math::inverse(parent_world_rot) * world_rot;

	// Calculate local scale
	Vec3 parent_world_scale = parent_transform->get_world_scale();
	Vec3 local_scale = world_scale / parent_world_scale;

	// Apply the new local transform
	node_transform->set_local_position(local_pos);
	node_transform->set_local_rotation(local_rot);
	node_transform->set_local_scale(local_scale);
}

/**
 * @brief Checks if a given entity is a descendant of a potential parent in the
 * scene graph.
 *
 * @param potentialParent The entity that is being checked as a potential
 * parent.
 * @param entityToCheck The entity that is being checked for being a descendant.
 * @return true if entityToCheck is a descendant of potentialParent, false
 * otherwise.
 */
std::vector<Entity> EntityManager::get_children(Entity parent) const {
	if (!parent.is_valid()) {
		return {};
	}
	const auto *node = parent.try_get_component<Components::SceneNodeComponent>();
	return node != nullptr ? node->children : std::vector<Entity>{};
}

std::optional<Entity> EntityManager::get_parent(Entity child) const {
	if (!child.is_valid()) {
		return std::nullopt;
	}
	const auto *node = child.try_get_component<Components::SceneNodeComponent>();
	if (node == nullptr || !node->parent.is_valid()) {
		return std::nullopt;
	}
	return node->parent;
}

bool EntityManager::is_descendant(Entity potential_parent, Entity entity_to_check) {
	auto *node = potential_parent.try_get_component<Components::SceneNodeComponent>();
	if (node == nullptr) {
		return false;
	}

	for (const auto &child : node->children) {
		if (child == entity_to_check) {
			return true;
		}
		if (is_descendant(child, entity_to_check)) {
			return true;
		}
	}
	return false;
}

/**
 * @brief Removes a child entity from its parent in the scene graph.
 *
 * @param parent The parent entity from which the child will be removed.
 * @param child The child entity to be removed.
 */
void EntityManager::remove_child(Entity parent, Entity child) {
	if (!parent.is_valid() || !child.is_valid()) {
		return;
	}

	auto *parent_node = parent.try_get_component<Components::SceneNodeComponent>();
	auto *child_node = child.try_get_component<Components::SceneNodeComponent>();
	if ((parent_node == nullptr) || (child_node == nullptr)) {
		return;
	}

	auto &siblings = parent_node->children;
	std::erase(siblings, child);

	child_node->parent = Entity::null();
}

/**
 * @brief Recursively removes all children of a given parent entity.
 *
 * @param parent The parent entity whose children will be removed.
 */
void EntityManager::remove_all_children(Entity parent) {
	if (!parent.is_valid()) {
		return;
	}

	auto *parent_node = parent.try_get_component<Components::SceneNodeComponent>();
	if (parent_node == nullptr) {
		return;
	}

	const auto children_copy = parent_node->children;
	for (const Entity &child : children_copy) {
		destroy_entity(child);
	}

	if (auto *node = parent.try_get_component<Components::SceneNodeComponent>()) {
		node->children.clear();
	}
}

} // namespace Aquila::SceneManagement
