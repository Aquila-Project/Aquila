#include "Aquila/Scene/Scene.h"

#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Photometry.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Scene/ComponentRegistry.h"
#include "Aquila/Scene/Components/FogComponent.h"
#include "Aquila/Scene/Components/FullscreenEffectComponent.h"
#include "Aquila/Scene/Components/LightComponent.h"
#include "Aquila/Scene/Components/MaterialComponent.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Scene/Components/SceneNodeComponent.h"
#include "Aquila/Scene/Components/SkyLightComponent.h"
#include "Aquila/Scene/Components/ScriptComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Graphics/Resources/MeshLibrary.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"

namespace Aquila::SceneManagement {

namespace {

using Graphics::Resources::MeshParams;
using Graphics::Resources::MeshSource;
using Graphics::Resources::MeshSourceKind;

constexpr int k_scene_version = 2;
constexpr int k_photometric_version = 2;

F32 legacy_light_scale() {
	return 1.F / Foundation::Photometry::exposure(Foundation::Photometry::DAYLIGHT_EV100);
}

F32 photometric_from_legacy(const Components::LightComponent &light) {
	using Type = Components::LightComponent::Type;
	const F32 scaled = light.m_intensity * legacy_light_scale();
	switch (light.m_type) {
	case Type::Point:
		return scaled / Foundation::Photometry::point_intensity(1.F);
	case Type::Spot:
		return scaled / Foundation::Photometry::spot_intensity(1.F, Math::radians(light.m_outer_cone_angle));
	default:
		return scaled;
	}
}

std::string snake_case(std::string_view name) {
	auto upper = [](char c) { return std::isupper(static_cast<unsigned char>(c)) != 0; };
	auto lower = [](char c) { return std::islower(static_cast<unsigned char>(c)) != 0; };
	std::string out;
	for (Usize i = 0; i < name.size(); ++i) {
		const char c = name[i];
		if (upper(c)) {
			const bool after_lower = i > 0 && lower(name[i - 1]);
			const bool acronym_end = i > 0 && upper(name[i - 1]) && i + 1 < name.size() && lower(name[i + 1]);
			if (after_lower || acronym_end) {
				out += '_';
			}
			out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		} else {
			out += c;
		}
	}
	return out;
}

nlohmann::ordered_json snake_case_keys(const nlohmann::ordered_json &value) {
	if (value.is_object()) {
		nlohmann::ordered_json out = nlohmann::ordered_json::object();
		for (const auto &[key, child] : value.items()) {
			out[snake_case(key)] = snake_case_keys(child);
		}
		return out;
	}
	if (value.is_array()) {
		nlohmann::ordered_json out = nlohmann::ordered_json::array();
		for (const auto &child : value) {
			out.push_back(snake_case_keys(child));
		}
		return out;
	}
	return value;
}

void upgrade_legacy_scene(nlohmann::ordered_json &scene_json) {
	if (!scene_json.contains("Entities") || scene_json.contains("entities")) {
		return;
	}
	nlohmann::ordered_json upgraded = nlohmann::ordered_json::object();
	upgraded["scene_name"] = scene_json.value("SceneName", "Untitled Scene");
	upgraded["entities"] = nlohmann::ordered_json::object();
	for (const auto &[id, entity] : scene_json["Entities"].items()) {
		upgraded["entities"][id]["components"] = snake_case_keys(entity);
	}
	scene_json = std::move(upgraded);
}

Graphics::MaterialData material_from_legacy_json(const nlohmann::ordered_json &json, bool legacy_light_units) {
	Graphics::MaterialData data;
	const std::string shader = json.value("shader", std::string());
	if (!shader.empty()) {
		data.shader = Graphics::portable_shader_path(shader);
	}
	auto read_vec4 = [&json](const char *key) -> Option<Vec4> {
		if (!json.contains(key) || !json[key].is_array() || json[key].size() < 4) {
			return std::nullopt;
		}
		const auto &v = json[key];
		return Vec4(v[0].get<F32>(), v[1].get<F32>(), v[2].get<F32>(), v[3].get<F32>());
	};
	if (Option<Vec4> albedo = read_vec4("albedo")) {
		data.values["baseColor"] = *albedo;
	}
	if (Option<Vec4> emissive = read_vec4("emissive")) {
		const F32 scale = legacy_light_units ? legacy_light_scale() : 1.F;
		data.values["emissiveColor"] = Vec3(*emissive);
		data.values["emissiveIntensity"] = emissive->w * scale;
	}
	auto read_float = [&json, &data](const char *key, const char *parameter) {
		if (json.contains(key) && json[key].is_number()) {
			data.values[parameter] = json[key].get<F32>();
		}
	};
	read_float("metallic", "metallic");
	read_float("roughness", "roughness");
	read_float("normal_strength", "normalStrength");
	read_float("ao_strength", "occlusionStrength");
	return data;
}

nlohmann::ordered_json mesh_source_to_json(const MeshSource &source) {
	switch (source.kind) {
	case MeshSourceKind::Procedural: {
		nlohmann::ordered_json params = nlohmann::ordered_json::object();
		for (const auto &[name, value] : source.params) {
			params[name] = value;
		}
		return { { "type", "procedural" }, { "generator", source.generator }, { "params", params } };
	}
	case MeshSourceKind::Asset:
		return { { "type", "asset" }, { "path", source.path } };
	case MeshSourceKind::None:
		break;
	}
	return nullptr;
}

Option<MeshSource> mesh_source_from_json(const nlohmann::ordered_json &mesh_json) {
	if (!mesh_json.contains("source")) {
		return MeshSource::from_legacy_path(mesh_json.value("path", ""));
	}
	const auto &source = mesh_json["source"];
	if (!source.is_object()) {
		return std::nullopt;
	}
	const std::string type = source.value("type", "");
	if (type == "procedural") {
		MeshParams params;
		if (source.contains("params") && source["params"].is_object()) {
			for (const auto &[name, value] : source["params"].items()) {
				if (value.is_number()) {
					params[name] = value.get<F32>();
				}
			}
		}
		return MeshSource::procedural(source.value("generator", ""), std::move(params));
	}
	if (type == "asset") {
		return MeshSource::asset(source.value("path", ""));
	}
	return std::nullopt;
}

Quaternion rotation_from_json(const nlohmann::ordered_json &rotation) {
	if (!rotation.is_array() || rotation.size() < 3) {
		return Quaternion(1.F, 0.F, 0.F, 0.F);
	}
	const Vec3 axis(rotation[0].get<F32>(), rotation[1].get<F32>(), rotation[2].get<F32>());
	const F32 w = rotation.size() >= 4 ? rotation[3].get<F32>() : Math::sqrt(Math::max(0.F, 1.F - Math::dot(axis, axis)));
	const Quaternion quaternion(w, axis.x, axis.y, axis.z);
	const F32 length = glm::length(quaternion);
	return length > Math::EPSILON ? quaternion / length : Quaternion(1.F, 0.F, 0.F, 0.F);
}

nlohmann::ordered_json script_fields_to_json(const std::map<std::string, Components::ScriptFieldValue> &fields) {
	nlohmann::ordered_json out = nlohmann::ordered_json::object();
	for (const auto &[name, value] : fields) {
		std::visit(
			[&out, &name](const auto &field) {
				using V = std::decay_t<decltype(field)>;
				if constexpr (std::is_same_v<V, Vec3>) {
					out[name] = { field.x, field.y, field.z };
				} else if constexpr (std::is_same_v<V, Components::EntityReference>) {
					out[name] = { { "entity", field.id.to_string() } };
				} else {
					out[name] = field;
				}
			},
			value);
	}
	return out;
}

std::map<std::string, Components::ScriptFieldValue> script_fields_from_json(const nlohmann::ordered_json &json) {
	std::map<std::string, Components::ScriptFieldValue> fields;
	if (!json.is_object()) {
		return fields;
	}
	for (const auto &[name, value] : json.items()) {
		if (value.is_boolean()) {
			fields[name] = value.get<bool>();
		} else if (value.is_number()) {
			fields[name] = value.get<F64>();
		} else if (value.is_string()) {
			fields[name] = value.get<std::string>();
		} else if (value.is_array() && value.size() == 3 &&
				   std::ranges::all_of(value, [](const auto &element) { return element.is_number(); })) {
			fields[name] = Vec3(value[0].get<F32>(), value[1].get<F32>(), value[2].get<F32>());
		} else if (value.is_object() && value.contains("entity") && value["entity"].is_string()) {
			fields[name] = Components::EntityReference{ Foundation::UUID::from_string(value["entity"].get<std::string>()) };
		}
	}
	return fields;
}

} // namespace
Scene::Scene() : m_scene_name("Unnamed scene"), m_scene_id(Foundation::UUID::generate()) {
	on_start();
}

Scene::Scene(std::string name) : m_scene_name(std::move(name)), m_scene_id(Foundation::UUID::generate()) {
	on_start();
}

Scene::~Scene() = default;

/**
 * @brief Retrieves the entt registry associated with the scene.
 *
 * @return entt::registry& The enthe scene starts, setting up the necessary
 * components for managing entities and their relationships within the scene.
 */
void Scene::on_start() {
	m_entity_manager = std::make_unique<EntityManager>(this);
	// Wire dirty callback on every TransformComponent that gets created.
	m_entity_manager->get_registry()
		.on_construct<Components::TransformComponent>()
		.connect<&Scene::on_transform_construct>(this);
	entt::registry &registry = m_entity_manager->get_registry();
	registry.on_construct<Components::MetadataComponent>().connect<&Scene::on_metadata_changed>(this);
	registry.on_update<Components::MetadataComponent>().connect<&Scene::on_metadata_changed>(this);
	registry.on_destroy<Components::MetadataComponent>().connect<&Scene::on_metadata_changed>(this);
	m_entity_manager->construct_scene_graph();
}

void Scene::on_transform_construct(entt::registry &registry, entt::entity e) {
	auto &t = registry.get<Components::TransformComponent>(e);
	t.set_dirty_callback([this, e]() { mark_transform_dirty(e); });
	mark_transform_dirty(e); // bootstrap: new entity needs its first world matrix compute
}

void Scene::on_metadata_changed(entt::registry &, entt::entity) {
	on_entities_changed();
}

void Scene::mark_transform_dirty(entt::entity entity) {
	m_dirty_transforms.mark_dirty(entity);
}

bool Scene::has_dirty_ancestor(entt::entity e) const {
	Entity entity(e, const_cast<Scene *>(this));
	auto *node = entity.try_get_component<Components::SceneNodeComponent>();
	while ((node != nullptr) && !node->parent.is_null()) {
		if (m_dirty_transforms.is_dirty(node->parent.get_handle())) {
			return true;
		}
		node = node->parent.try_get_component<Components::SceneNodeComponent>();
	}
	return false;
}

int Scene::get_entity_depth(entt::entity e) const {
	int depth = 0;
	Entity entity(e, const_cast<Scene *>(this));
	auto *node = entity.try_get_component<Components::SceneNodeComponent>();
	while ((node != nullptr) && !node->parent.is_null()) {
		depth++;
		node = node->parent.try_get_component<Components::SceneNodeComponent>();
	}
	return depth;
}

/**
 * @brief Retrieves the entt registry associated with the scene.
 *
 * @return entt::registry& The entt registry for the scene, which contains all
 * entities and their components.
 */
entt::registry &Scene::get_registry() const {
	return m_entity_manager->get_registry();
}

/**
 * @brief Retrieves the EntityManager associated with the scene.
 *
 * @return EntityManager* Pointer to the EntityManager that manages entities in
 * the scene.
 */
EntityManager *Scene::get_entity_manager() const {
	return m_entity_manager.get();
}

/**
 * @brief Retrieves the name of the scene.
 *
 * @return const std::string& The name of the scene.
 */
const std::string &Scene::get_scene_name() const {
	return m_scene_name;
}

Foundation::UUID Scene::get_handle() const {
	return m_scene_id;
}

void Scene::clear() {
	m_entity_manager->clear();
	m_active_camera_entity = entt::null;
	m_dirty_transforms.clear();
}

void Scene::copy_from(const Scene &other) {
	if (&other == this) {
		return;
	}
	clear();
	m_scene_name = other.m_scene_name;

	entt::registry &source = other.get_registry();
	entt::registry &target = get_registry();
	for (auto [handle] : source.storage<entt::entity>().each()) {
		[[maybe_unused]] const entt::entity created = target.create(handle);
		AQUILA_ASSERT(created == handle, "Scene::copy_from needs a cleared registry to keep entity ids");
	}

	const ComponentRegistry &components = ComponentRegistry::instance();
	Scene *source_scene = const_cast<Scene *>(&other);
	for (auto [handle] : source.storage<entt::entity>().each()) {
		components.copy_components(Entity(handle, source_scene), Entity(handle, this));
	}

	const auto here = [this](const Entity &entity) {
		return entity.is_null() ? Entity::null() : Entity(entity.get_handle(), this);
	};
	for (auto [handle, node] : target.view<Components::SceneNodeComponent>().each()) {
		const auto *original = source.try_get<Components::SceneNodeComponent>(handle);
		node.ent = Entity(handle, this);
		node.parent = original != nullptr ? here(original->parent) : Entity::null();
		node.children.clear();
		if (original != nullptr) {
			for (const Entity &child : original->children) {
				node.children.push_back(here(child));
			}
		}
	}

	m_active_camera_entity = other.m_active_camera_entity;
}

void Scene::update_transform_hierarchy() {
	if (m_dirty_transforms.is_empty()) {
		return;
	}

	// Sort dirty entities parent-first so ancestors are always processed before descendants.
	std::vector<entt::entity> sorted = m_dirty_transforms.get_ordered();
	std::ranges::stable_sort(
		sorted, [this](entt::entity a, entt::entity b) { return get_entity_depth(a) < get_entity_depth(b); });

	for (entt::entity e : sorted) {
		// If a dirty ancestor is also in the set, it was (or will be) processed first and its
		// UpdateTransformRecursive call already covers this entity — skip it.
		if (has_dirty_ancestor(e)) {
			continue;
		}

		Entity entity(e, this);
		Mat4 parent_world(1.0F);
		auto *node = entity.try_get_component<Components::SceneNodeComponent>();
		if ((node != nullptr) && !node->parent.is_null()) {
			auto *parent_transform = node->parent.try_get_component<Components::TransformComponent>();
			if (parent_transform != nullptr) {
				parent_world = parent_transform->get_world_matrix_lazy();
			}
		}
		update_transform_recursive(entity, parent_world);
	}

	m_dirty_transforms.clear();
}

void Scene::update_transform_recursive(Entity entity, const Mat4 &parent_world) {
	if (!entity.is_valid()) {
		return;
	}

	auto *transform = entity.try_get_component<Components::TransformComponent>();
	if (transform == nullptr) {
		return;
	}

	// Update this entity's world matrix
	transform->update_world_matrix(parent_world);

	// Recursively update all children
	if (auto *node = entity.try_get_component<Components::SceneNodeComponent>()) {
		for (auto child : node->children) {
			if (child.is_valid()) {
				update_transform_recursive(child, transform->get_world_matrix());
			}
		}
	}
}

void Scene::set_active_camera(Entity camera_entity) {
	if (camera_entity.is_valid() && camera_entity.has_component<Components::CameraComponent>()) {
		// Store the raw entt::entity handle
		m_active_camera_entity = camera_entity.get_handle();
	}
}

Entity Scene::get_active_camera_entity() const {
	// Wrap the entt::entity in an Entity object
	if (m_active_camera_entity != entt::null) {
		return { m_active_camera_entity, const_cast<Scene *>(this) };
	}
	return {}; // Return invalid entity
}

bool Scene::has_active_camera() const {
	if (m_active_camera_entity == entt::null) {
		return false;
	}

	// Check if entity still exists and has camera component
	return get_registry().valid(m_active_camera_entity) &&
		get_registry().all_of<Components::CameraComponent>(m_active_camera_entity);
}

Entity Scene::find_primary_camera() const {
	// Iterate through all entities with CameraComponent
	auto view = get_registry().view<Components::CameraComponent>();

	for (auto entity : view) {
		const auto &cam = view.get<Components::CameraComponent>(entity);
		if (cam.primary) {
			return { entity, const_cast<Scene *>(this) };
		}
	}
	return {}; // Return invalid entity if no primary camera found
}

Entity Scene::find_game_camera() const {
	if (Entity primary = find_primary_camera(); primary.is_valid()) {
		return primary;
	}
	auto cameras = get_registry().view<Components::CameraComponent>();
	return cameras.empty() ? Entity{} : Entity(cameras.front(), const_cast<Scene *>(this));
}

bool Scene::serialize(const std::string &filepath) {
	nlohmann::ordered_json scene_json;

	scene_json["scene_name"] = m_scene_name;
	scene_json["version"] = k_scene_version;

	auto &registry = get_registry();
	scene_json["entities"] = nlohmann::ordered_json::object();

	auto view = registry.view<Components::MetadataComponent>();

	for (auto entity_handle : view) {
		Entity entity(entity_handle, this);
		nlohmann::ordered_json entity_json;
		entity_json["components"] = nlohmann::ordered_json::object();

		// Serialize MetadataComponent
		if (entity.has_component<Components::MetadataComponent>()) {
			auto &meta = entity.get_component<Components::MetadataComponent>();
			entity_json["components"]["metadata_component"] = { { "name", meta.get_name() },
																{ "uuid", meta.get_id().to_string() },
																{ "enabled", meta.is_visible() },
																{ "selected", meta.is_selected() } };
		}

		if (entity.has_component<Components::ScriptComponent>()) {
			auto &script_comp = entity.get_component<Components::ScriptComponent>();
			entity_json["components"]["script_component"] = {
				{ "path", script_comp.path },
				{ "fields", script_fields_to_json(script_comp.fields) },
			};
		}

		// Serialize TransformComponent
		if (entity.has_component<Components::TransformComponent>()) {
			auto &transform = entity.get_component<Components::TransformComponent>();
			entity_json["components"]["transform_component"] = {
				{
					"position",
					{
						transform.get_local_position().x,
						transform.get_local_position().y,
						transform.get_local_position().z,
					},
				},
				{
					"rotation",
					{
						transform.get_local_rotation().x,
						transform.get_local_rotation().y,
						transform.get_local_rotation().z,
						transform.get_local_rotation().w,
					},
				},
				{
					"scale",
					{
						transform.get_local_scale().x,
						transform.get_local_scale().y,
						transform.get_local_scale().z,
					},
				},
			};
		}

		// Serialize SceneNodeComponent
		if (entity.has_component<Components::SceneNodeComponent>()) {
			auto &node = entity.get_component<Components::SceneNodeComponent>();
			entity_json["components"]["scene_node_component"] = {
				{
					"parent",
					node.parent.is_null()
						? "null"
						: node.parent.get_component<Components::MetadataComponent>().get_id().to_string(),
				},
				{ "children", nlohmann::ordered_json::array() },
			};

			for (auto &child : node.children) {
				if (!child.is_null() && child.has_component<Components::MetadataComponent>()) {
					entity_json["components"]["scene_node_component"]["children"].push_back(
						child.get_component<Components::MetadataComponent>().get_id().to_string());
				}
			}
		}

		// Serialize MeshComponent
		if (entity.has_component<Components::MeshComponent>()) {
			auto &mesh = entity.get_component<Components::MeshComponent>();
			nlohmann::ordered_json mesh_json = nlohmann::ordered_json::object();
			if (mesh.data && mesh.data->get_source().kind != MeshSourceKind::None) {
				mesh_json["source"] = mesh_source_to_json(mesh.data->get_source());
			}
			mesh_json["cast_shadows"] = mesh.cast_shadows;
			mesh_json["receive_shadows"] = mesh.receive_shadows;
			entity_json["components"]["mesh_component"] = std::move(mesh_json);
		}

		// Serialize LightComponent with shadow settings
		if (entity.has_component<Components::LightComponent>()) {
			auto &light = entity.get_component<Components::LightComponent>();

			entity_json["components"]["light_component"] = {
				{ "type", static_cast<int>(light.m_type) },
				{ "color", { light.m_color.r, light.m_color.g, light.m_color.b } },
				{ "intensity", light.m_intensity },
				{ "range", light.m_range },
				{ "inner_cone_angle", light.m_inner_cone_angle },
				{ "outer_cone_angle", light.m_outer_cone_angle },
				{ "direction", { light.m_direction.x, light.m_direction.y, light.m_direction.z } },
				{ "source_radius", light.m_source_radius },
				{ "angular_diameter", light.m_angular_diameter },
				{ "use_temperature", light.m_use_temperature },
				{ "temperature", light.m_temperature },
				{ "intensity_unit", static_cast<int>(light.m_intensity_unit) },
				{ "cast_shadows", light.m_cast_shadows },
				{ "is_active", light.m_is_active },
			};

			// Serialize shadow settings for directional lights
			if (light.m_type == Components::LightComponent::Type::Directional) {
				const auto &shadow_settings = light.m_shadow_settings;
				entity_json["components"]["light_component"]["shadow_settings"] = {
					{ "light_size", shadow_settings.light_size },
					{ "shadow_bias", shadow_settings.shadow_bias },
					{ "normal_bias", shadow_settings.normal_bias },
					{ "pcf_samples", shadow_settings.pcf_samples },
					{ "cascade_split_lambda", shadow_settings.cascade_split_lambda },
					{ "blocker_search_samples", shadow_settings.blocker_search_samples },
				};
			}
		}

		// Serialize CameraComponent
		if (entity.has_component<Components::CameraComponent>()) {
			auto &cam = entity.get_component<Components::CameraComponent>();
			entity_json["components"]["camera_component"] = {
				{ "primary", cam.primary },
				{ "is_orthographic", cam.is_orthographic },
				{ "fov", cam.fov },
				{ "aspect_ratio", cam.aspect_ratio },
				{ "near_plane", cam.near_plane },
				{ "far_plane", cam.far_plane },
				{ "ev100", cam.ev100 },
				{ "auto_exposure", cam.auto_exposure },
				{ "exposure_compensation", cam.exposure_compensation },
				{ "bloom", cam.bloom },
				{ "bloom_intensity", cam.bloom_intensity },
				{ "ortho_left", cam.ortho_left },
				{ "ortho_right", cam.ortho_right },
				{ "ortho_top", cam.ortho_top },
				{ "ortho_bottom", cam.ortho_bottom },
			};
		}

		// Serialize SkyLightComponent
		if (entity.has_component<Components::SkyLightComponent>()) {
			const auto &sky = entity.get_component<Components::SkyLightComponent>();
			const Vec3 tint = sky.get_tint();
			const Vec3 ground = sky.get_ground_albedo();
			entity_json["components"]["sky_light_component"] = {
				{ "active", sky.is_active() },
				{ "source", static_cast<int>(sky.get_source()) },
				{ "sun_elevation", sky.get_sun_elevation() },
				{ "sun_azimuth", sky.get_sun_azimuth() },
				{ "turbidity", sky.get_turbidity() },
				{ "ground_albedo", { ground.x, ground.y, ground.z } },
				{ "intensity", sky.get_intensity() },
				{ "tint", { tint.x, tint.y, tint.z } },
				{ "render_skybox", sky.get_render_skybox() },
				{ "skybox_lod", sky.get_skybox_lod() },
			};
		}

		if (entity.has_component<Components::FullscreenEffectComponent>()) {
			const auto &effect = entity.get_component<Components::FullscreenEffectComponent>();
			entity_json["components"]["fullscreen_effect_component"] = {
				{ "material", effect.material },
				{ "order", effect.order },
				{ "active", effect.active },
			};
		}

		if (entity.has_component<Components::FogComponent>()) {
			const auto &fog = entity.get_component<Components::FogComponent>();
			entity_json["components"]["fog_component"] = {
				{ "density", fog.density },
				{ "height_falloff", fog.height_falloff },
				{ "base_height", fog.base_height },
				{ "color", { fog.color.r, fog.color.g, fog.color.b } },
				{ "anisotropy", fog.anisotropy },
				{ "max_opacity", fog.max_opacity },
				{ "start_distance", fog.start_distance },
				{ "active", fog.active },
			};
		}

		// Serialize MaterialComponent
		if (entity.has_component<Components::MaterialComponent>()) {
			auto &mat_comp = entity.get_component<Components::MaterialComponent>();
			nlohmann::ordered_json material_json = nlohmann::ordered_json::object();
			if (mat_comp.uses_asset()) {
				material_json["asset"] = mat_comp.asset;
			} else {
				if (mat_comp.material && mat_comp.material->get_asset_path().empty()) {
					mat_comp.embedded = mat_comp.material->get_data();
				}
				material_json["material"] = Graphics::material_to_json(mat_comp.embedded);
			}
			entity_json["components"]["material_component"] = std::move(material_json);
		}

		scene_json["entities"][std::to_string(static_cast<int>(entity_handle))] = entity_json;
	}

	const auto vfs_file = Aquila::Platform::Filesystem::VirtualFileSystem::get()->open_file(filepath, AccessMode::Write,
																							OpenMode::Binary);
	if (!vfs_file || !vfs_file->is_valid()) {
		AQUILA_LOG_ERROR("VFS could not open file. Verify you are passing a VIRTUAL file and not a folder");
		return false;
	}

	vfs_file->write(scene_json.dump(4).data(), scene_json.dump(4).size());
	vfs_file->close();

	return true;
}

bool Scene::deserialize(const std::string &filepath) {
	AQUILA_ASSERT(m_entity_manager != nullptr, "EntityManager is nullptr");

	auto vfs_file =
		Aquila::Platform::Filesystem::VirtualFileSystem::get()->open_file(filepath, AccessMode::Read, OpenMode::Binary);
	if (!vfs_file || !vfs_file->is_valid()) {
		return false;
	}

	std::vector<char> buffer(vfs_file->size());
	vfs_file->read(buffer.data(), buffer.size());
	vfs_file->close();

	nlohmann::ordered_json scene_json = nlohmann::ordered_json::parse(buffer.begin(), buffer.end(), nullptr, false);
	if (scene_json.is_discarded() || !scene_json.is_object()) {
		AQUILA_LOG_ERROR("Scene: {} is not valid JSON", filepath);
		return false;
	}
	upgrade_legacy_scene(scene_json);

	if (!scene_json.contains("entities")) {
		return false;
	}

	m_scene_name = scene_json.value("scene_name", "Untitled Scene");
	const bool legacy_light_units = scene_json.value("version", 1) < k_photometric_version;
	clear();

	const auto &entities_json = scene_json["entities"];

	std::unordered_map<std::string, Entity> uuid_to_entity;

	// First pass: Create all entities with metadata
	for (auto &[id_str, entity_data] : entities_json.items()) {
		if (entity_data.contains("components")) {
			const auto &components = entity_data["components"];
			if (components.contains("metadata_component")) {
				const auto &meta = components["metadata_component"];
				const std::string uuid = meta.value("uuid", "");
				Entity entity = m_entity_manager->create_entity(meta.value("name", ""));
				auto &metadata = entity.get_component<Components::MetadataComponent>();
				if (!uuid.empty()) {
					metadata.set_id(Foundation::UUID::from_string(uuid));
				}
				metadata.set_visible(meta.value("enabled", true));
				metadata.set_selected(meta.value("selected", false));
				uuid_to_entity[uuid] = entity;
			}
		}
	}

	// Second pass: Add all components
	for (auto &[id_str, entity_data] : entities_json.items()) {
		if (!entity_data.contains("components")) {
			continue;
		}

		const auto &components = entity_data["components"];

		if (!components.contains("metadata_component")) {
			continue;
		}

		std::string uuid_str = components["metadata_component"].value("uuid", "");
		if (!uuid_to_entity.contains(uuid_str)) {
			continue;
		}
		Entity entity = uuid_to_entity.at(uuid_str);

		// Deserialize TransformComponent
		if (components.contains("transform_component")) {
			const auto &transform_json = components["transform_component"];
			Vec3 position =
				Vec3(transform_json["position"][0], transform_json["position"][1], transform_json["position"][2]);
			Vec3 scale = Vec3(transform_json["scale"][0], transform_json["scale"][1], transform_json["scale"][2]);

			auto &transform = entity.get_or_emplace<Components::TransformComponent>();
			transform.set_local_position(position);
			transform.set_local_rotation(rotation_from_json(transform_json["rotation"]));
			transform.set_local_scale(scale);
			transform.update_world_matrix();
		}

		// Deserialize SceneNodeComponent
		if (components.contains("scene_node_component")) {
			const auto &node_json = components["scene_node_component"];
			Components::SceneNodeComponent node;
			node.ent = entity;

			if (std::string parent_uuid = node_json.value("parent", "null"); parent_uuid == "null") {
				node.parent = Entity::null();
			} else {
				node.parent = uuid_to_entity.contains(parent_uuid) ? uuid_to_entity.at(parent_uuid) : Entity::null();
			}

			if (node_json.contains("children")) {
				for (const auto &child_uuid_json : node_json["children"]) {
					if (auto child_uuid = child_uuid_json.get<std::string>(); uuid_to_entity.contains(child_uuid)) {
						node.children.push_back(uuid_to_entity.at(child_uuid));
					}
				}
			}

			entity.add_or_replace_component<Components::SceneNodeComponent>(node);
		}

		if (components.contains("script_component")) {
			const auto &script_json = components["script_component"];
			Components::ScriptComponent script{ .path = script_json.value("path", "") };
			if (script_json.contains("fields")) {
				script.fields = script_fields_from_json(script_json["fields"]);
			}
			entity.add_or_replace_component<Components::ScriptComponent>(std::move(script));
		}

		// Deserialize MeshComponent
		if (components.contains("mesh_component")) {
			const auto &mesh_json = components["mesh_component"];
			auto &mesh_comp = entity.get_or_emplace<Components::MeshComponent>();

			if (Option<MeshSource> source = mesh_source_from_json(mesh_json)) {
				if (Ref<Graphics::Resources::Mesh> mesh = Graphics::Resources::MeshLibrary::instance().load(*source)) {
					mesh_comp.set_mesh(mesh);
				}
			}

			mesh_comp.cast_shadows = mesh_json.value("cast_shadows", true);
			mesh_comp.receive_shadows = mesh_json.value("receive_shadows", true);
		}

		// Deserialize LightComponent with shadow settings
		if (components.contains("light_component")) {
			const auto &light_json = components["light_component"];
			Components::LightComponent light;

			if (light_json.contains("type")) {
				light.m_type = static_cast<Components::LightComponent::Type>(light_json["type"].get<int>());
			}

			if (light_json.contains("color")) {
				light.m_color = Vec3(light_json["color"][0].get<F32>(), light_json["color"][1].get<F32>(),
									 light_json["color"][2].get<F32>());
			}

			if (light_json.contains("intensity")) {
				light.m_intensity = light_json["intensity"].get<F32>();
			}
			if (light_json.contains("range")) {
				light.m_range = light_json["range"].get<F32>();
			}
			if (light_json.contains("inner_cone_angle")) {
				light.m_inner_cone_angle = light_json["inner_cone_angle"].get<F32>();
			}
			if (light_json.contains("outer_cone_angle")) {
				light.m_outer_cone_angle = light_json["outer_cone_angle"].get<F32>();
			}

			if (light_json.contains("direction")) {
				light.m_direction = Vec3(light_json["direction"][0].get<F32>(), light_json["direction"][1].get<F32>(),
										 light_json["direction"][2].get<F32>());
			}

			if (light_json.contains("is_active")) {
				light.m_is_active = light_json["is_active"].get<bool>();
			}

			light.m_source_radius = light_json.value("source_radius", light.m_source_radius);
			light.m_angular_diameter = light_json.value("angular_diameter", light.m_angular_diameter);
			light.m_use_temperature = light_json.value("use_temperature", light.m_use_temperature);
			light.m_temperature = light_json.value("temperature", light.m_temperature);
			light.m_intensity_unit = static_cast<Components::LightComponent::IntensityUnit>(
				light_json.value("intensity_unit", static_cast<int>(light.m_intensity_unit)));
			light.m_cast_shadows = light_json.value("cast_shadows", light.m_cast_shadows);

			if (legacy_light_units && light_json.contains("intensity")) {
				light.m_intensity = photometric_from_legacy(light);
			}

			// Deserialize shadow settings for directional lights
			if (light.m_type == Components::LightComponent::Type::Directional &&
				light_json.contains("shadow_settings")) {
				const auto &shadow_json = light_json["shadow_settings"];

				light.m_shadow_settings.light_size = shadow_json.value("light_size", 8.0F);
				light.m_shadow_settings.shadow_bias = shadow_json.value("shadow_bias", 0.0005F);
				light.m_shadow_settings.normal_bias = shadow_json.value("normal_bias", 1.0F);
				light.m_shadow_settings.pcf_samples = shadow_json.value("pcf_samples", 32);
				light.m_shadow_settings.cascade_split_lambda = shadow_json.value("cascade_split_lambda", 0.95F);
				light.m_shadow_settings.blocker_search_samples = shadow_json.value("blocker_search_samples", 16);
			}

			entity.add_or_replace_component<Components::LightComponent>(light);
		}

		// Deserialize CameraComponent
		if (components.contains("camera_component")) {
			const auto &cam_json = components["camera_component"];
			Components::CameraComponent cam;

			cam.primary = cam_json.value("primary", false);
			cam.is_orthographic = cam_json.value("is_orthographic", false);
			cam.fov = cam_json.value("fov", 45.0F);
			cam.aspect_ratio = cam_json.value("aspect_ratio", 16.0F / 9.0F);
			cam.near_plane = cam_json.value("near_plane", 0.1F);
			cam.far_plane = cam_json.value("far_plane", 1000.0F);
			cam.ev100 = cam_json.value("ev100", Foundation::Photometry::DAYLIGHT_EV100);
			cam.auto_exposure = cam_json.value("auto_exposure", true);
			cam.exposure_compensation = cam_json.value("exposure_compensation", 0.F);
			cam.bloom = cam_json.value("bloom", true);
			cam.bloom_intensity = cam_json.value("bloom_intensity", 1.F);
			cam.ortho_left = cam_json.value("ortho_left", -10.0F);
			cam.ortho_right = cam_json.value("ortho_right", 10.0F);
			cam.ortho_top = cam_json.value("ortho_top", 10.0F);
			cam.ortho_bottom = cam_json.value("ortho_bottom", -10.0F);

			entity.add_or_replace_component<Components::CameraComponent>(cam);
		}

		// Deserialize SkyLightComponent
		if (components.contains("sky_light_component")) {
			const auto &sky_light_json = components["sky_light_component"];
			auto &sky_light = entity.get_or_emplace<Components::SkyLightComponent>();

			sky_light.set_active(sky_light_json.value("active", true));
			sky_light.set_source(static_cast<Components::SkySource>(sky_light_json.value("source", 0)));
			sky_light.set_sun_elevation(sky_light_json.value("sun_elevation", sky_light.get_sun_elevation()));
			sky_light.set_sun_azimuth(sky_light_json.value("sun_azimuth", sky_light.get_sun_azimuth()));
			sky_light.set_turbidity(sky_light_json.value("turbidity", sky_light.get_turbidity()));
			sky_light.set_skybox_lod(sky_light_json.value("skybox_lod", sky_light.get_skybox_lod()));
			sky_light.set_intensity(sky_light_json.value("intensity", 1.0F));

			if (sky_light_json.contains("ground_albedo")) {
				const auto &ground = sky_light_json["ground_albedo"];
				sky_light.set_ground_albedo(Vec3(ground[0].get<F32>(), ground[1].get<F32>(), ground[2].get<F32>()));
			}

			if (sky_light_json.contains("tint")) {
				Vec3 tint = Vec3(sky_light_json["tint"][0].get<F32>(), sky_light_json["tint"][1].get<F32>(),
								 sky_light_json["tint"][2].get<F32>());
				sky_light.set_tint(tint);
			}

			sky_light.set_render_skybox(sky_light_json.value("render_skybox", true));
		}

		if (components.contains("fullscreen_effect_component")) {
			const auto &effect_json = components["fullscreen_effect_component"];
			Components::FullscreenEffectComponent effect;
			effect.material = effect_json.value("material", std::string());
			effect.order = effect_json.value("order", 0);
			effect.active = effect_json.value("active", true);
			entity.add_or_replace_component<Components::FullscreenEffectComponent>(effect);
		}

		if (components.contains("fog_component")) {
			const auto &fog_json = components["fog_component"];
			Components::FogComponent fog;
			fog.density = fog_json.value("density", fog.density);
			fog.height_falloff = fog_json.value("height_falloff", fog.height_falloff);
			fog.base_height = fog_json.value("base_height", fog.base_height);
			if (fog_json.contains("color")) {
				fog.color = Vec3(fog_json["color"][0].get<F32>(), fog_json["color"][1].get<F32>(),
								 fog_json["color"][2].get<F32>());
			}
			fog.anisotropy = fog_json.value("anisotropy", fog.anisotropy);
			fog.max_opacity = fog_json.value("max_opacity", fog.max_opacity);
			fog.start_distance = fog_json.value("start_distance", fog.start_distance);
			fog.active = fog_json.value("active", fog.active);
			entity.add_or_replace_component<Components::FogComponent>(fog);
		}

		// Deserialize MaterialComponent
		if (components.contains("material_component")) {
			const auto &mat_json = components["material_component"];
			auto &mat_comp = entity.get_or_emplace<Components::MaterialComponent>();
			mat_comp.material = nullptr;
			mat_comp.asset = mat_json.value("asset", std::string());
			if (mat_json.contains("material")) {
				mat_comp.embedded = Graphics::material_from_json(mat_json["material"]);
			} else if (!mat_comp.uses_asset()) {
				mat_comp.embedded = material_from_legacy_json(mat_json, legacy_light_units);
			}
		}
	}

	if (Entity camera = find_primary_camera(); camera.is_valid()) {
		set_active_camera(camera);
	}
	on_loaded();
	return true;
}
} // namespace Aquila::SceneManagement
