#include "Aquila/Scene/SceneStatistics.h"

#include "Aquila/Graphics/Resources/Mesh.h"
#include "Aquila/Scene/Components/LightComponent.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/MetadataComponent.h"
#include "Aquila/Scene/Scene.h"

#include <bit>

namespace Aquila::SceneManagement {

using namespace Aquila::SceneManagement::Components;

namespace {

using PositionKey = std::array<Uint32, 3>;

PositionKey key_of(const Vec3 &position) {
	return { std::bit_cast<Uint32>(position.x), std::bit_cast<Uint32>(position.y), std::bit_cast<Uint32>(position.z) };
}

Uint64 count_unique_positions(const Graphics::Resources::Mesh &mesh) {
	std::vector<PositionKey> keys;
	keys.reserve(mesh.get_vertices().size());
	for (const auto &vertex : mesh.get_vertices()) {
		keys.push_back(key_of(vertex.pos));
	}
	std::ranges::sort(keys);
	return static_cast<Uint64>(std::ranges::unique(keys).begin() - keys.begin());
}

void add(GeometryStatistics &into, const GeometryStatistics &counts) {
	into.vertices += counts.vertices;
	into.triangles += counts.triangles;
}

} // namespace

GeometryStatistics StatisticsCollector::geometry_of(const Graphics::Resources::Mesh &mesh) {
	CachedMesh &cached = m_cache[&mesh];
	if (cached.vertex_count != mesh.get_vertex_count() || cached.index_count != mesh.get_index_count()) {
		cached.vertex_count = mesh.get_vertex_count();
		cached.index_count = mesh.get_index_count();
		cached.counts.vertices = count_unique_positions(mesh);
		const Uint32 index_source = mesh.has_index_buffer() ? mesh.get_index_count() : mesh.get_vertex_count();
		cached.counts.triangles = index_source / 3;
	}
	return cached.counts;
}

SceneStatistics StatisticsCollector::collect(Scene &scene, Entity selected) {
	SceneStatistics statistics;
	auto &registry = scene.get_registry();

	statistics.objects = static_cast<Uint32>(registry.view<MetadataComponent>().size());
	statistics.lights = static_cast<Uint32>(registry.view<LightComponent>().size());

	const bool has_selection = selected.is_valid() && selected.exists();
	statistics.selected_objects = has_selection ? 1U : 0U;

	for (auto [handle, mesh_component] : registry.view<MeshComponent>().each()) {
		if (!mesh_component.is_valid()) {
			continue;
		}
		++statistics.meshes;

		const GeometryStatistics counts = geometry_of(*mesh_component.data);
		add(statistics.total, counts);
		if (has_selection && handle == selected.get_handle()) {
			add(statistics.selected, counts);
		}
	}

	return statistics;
}

} // namespace Aquila::SceneManagement
