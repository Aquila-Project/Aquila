#ifndef AQUILA_MESH_H
#define AQUILA_MESH_H

#include "Aquila/RHI/Vertex.h"
#include "Aquila/RHI/GPUMesh.h"
#include "Aquila/Graphics/Resources/MeshSource.h"

namespace Aquila::Graphics::Resources {

struct MeshData {
	std::vector<RHI::Vertex> vertices;
	std::vector<Uint32> indices;
	/// One per sub-mesh. Empty means a single primitive covering everything.
	std::vector<RHI::GPUMeshPrimitive> primitives;
};

/// Where a mesh's surface is and how big it is, in the mesh's own space. `axis_area` splits the area by how much each
/// triangle faces each axis (its area times its normal's squared components, the three add up to the whole area), so
/// a scaled copy's area can be worked out without going over the triangles again.
struct MeshSurface {
	Vec3 center = Vec3(0.F);
	Vec3 axis_area = Vec3(0.F);

	[[nodiscard]] F32 get_area() const { return axis_area.x + axis_area.y + axis_area.z; }
	/// Area after scaling by `scale` (a transform's per-axis scale). Exact for uniform scales and for faces lined up
	/// with the axes, a little under for slanted faces scaled unevenly.
	[[nodiscard]] F32 get_scaled_area(const Vec3 &scale) const {
		return scale.y * scale.z * axis_area.x + scale.x * scale.z * axis_area.y + scale.x * scale.y * axis_area.z;
	}
};

/// CPU side mesh data. Every change bumps get_revision(), so whoever owns the GPU copy knows to re-upload.
/// Meshes from MeshLibrary are shared between entities: don't edit them in place, load another source instead.
class Mesh {
  public:
	explicit Mesh(std::string debug_name);
	~Mesh() = default;

	AQUILA_NONCOPYABLE(Mesh);

	/// `source` is what scenes save for this mesh, leave it None for meshes made in code.
	void set_data(MeshData data, MeshSource source = {});

	/// Points into this mesh, use it before the mesh changes or dies.
	[[nodiscard]] RHI::GPUMeshDesc to_gpu_desc() const {
		RHI::GPUMeshDesc desc;
		desc.vertices = &m_vertices;
		desc.indices = has_index_buffer() ? &m_indices : nullptr;
		desc.primitives = &m_primitives;
		desc.debug_name = m_debug_name;
		return desc;
	}

	// Getters
	[[nodiscard]] const MeshSource &get_source() const { return m_source; }
	[[nodiscard]] const std::string &get_debug_name() const { return m_debug_name; }
	[[nodiscard]] const std::vector<RHI::Vertex> &get_vertices() const { return m_vertices; }
	[[nodiscard]] const std::vector<Uint32> &get_indices() const { return m_indices; }
	[[nodiscard]] const std::vector<RHI::GPUMeshPrimitive> &get_primitives() const { return m_primitives; }
	[[nodiscard]] Uint32 get_vertex_count() const { return static_cast<Uint32>(m_vertices.size()); }
	[[nodiscard]] Uint32 get_index_count() const { return static_cast<Uint32>(m_indices.size()); }
	[[nodiscard]] bool has_index_buffer() const { return !m_indices.empty(); }
	[[nodiscard]] Uint64 get_revision() const { return m_revision; }
	/// Measured by set_data(). `center` is the middle of the bounding box.
	[[nodiscard]] const MeshSurface &get_surface() const { return m_surface; }

  private:
	std::string m_debug_name;
	MeshSource m_source;
	MeshSurface m_surface;

	std::vector<RHI::Vertex> m_vertices;
	std::vector<Uint32> m_indices;
	std::vector<RHI::GPUMeshPrimitive> m_primitives;

	Uint64 m_revision = 0;
};

} // namespace Aquila::Graphics::Resources
#endif
