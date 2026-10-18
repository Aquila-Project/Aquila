#include "Aquila/Graphics/Resources/Mesh.h"
#include "Aquila/Foundation/Math/Math.h"

namespace Aquila::Graphics::Resources {

namespace {

MeshSurface measure_surface(const std::vector<RHI::Vertex> &vertices, const std::vector<Uint32> &indices) {
	MeshSurface surface;
	if (vertices.empty()) {
		return surface;
	}

	Vec3 low(std::numeric_limits<F32>::max());
	Vec3 high(std::numeric_limits<F32>::lowest());
	for (const RHI::Vertex &vertex : vertices) {
		low = Math::min(low, vertex.pos);
		high = Math::max(high, vertex.pos);
	}
	surface.center = (low + high) * 0.5F;

	const Usize corner_count = indices.empty() ? vertices.size() : indices.size();
	const auto corner = [&](Usize i) -> const Vec3 & {
		return vertices[indices.empty() ? i : indices[i]].pos;
	};
	for (Usize i = 0; i + 2 < corner_count; i += 3) {
		if (!indices.empty() && (indices[i] >= vertices.size() || indices[i + 1] >= vertices.size() ||
								 indices[i + 2] >= vertices.size())) {
			continue;
		}
		const Vec3 doubled = Math::cross(corner(i + 1) - corner(i), corner(i + 2) - corner(i));
		const F32 length = Math::length(doubled);
		if (length > 0.F) {
			surface.axis_area += doubled * doubled / (2.F * length);
		}
	}
	return surface;
}

}

Mesh::Mesh(std::string debug_name) : m_debug_name(std::move(debug_name)) {}

void Mesh::set_data(MeshData data, MeshSource source) {
	m_vertices = std::move(data.vertices);
	m_indices = std::move(data.indices);
	m_primitives = std::move(data.primitives);
	m_source = std::move(source);
	m_surface = measure_surface(m_vertices, m_indices);

	if (m_primitives.empty() && !m_vertices.empty()) {
		m_primitives.push_back({
			.first_index = 0,
			.first_vertex = 0,
			.index_count = get_index_count(),
			.vertex_count = get_vertex_count(),
		});
	}
	++m_revision;
}

} // namespace Aquila::Graphics::Resources
