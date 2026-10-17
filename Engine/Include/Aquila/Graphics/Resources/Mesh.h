#ifndef AQUILA_MESH_H
#define AQUILA_MESH_H

#include "Aquila/RHI/Vertex.h"
#include "Aquila/RHI/GPUMesh.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"

#include "Aquila/Foundation/Math/Math.h"

namespace Aquila::Graphics::Resources {

struct MeshData {
	std::vector<RHI::Vertex> vertices;
	std::vector<Uint32> indices;
	std::string path;
};

/// CPU side mesh data. Every change bumps get_revision(), so whoever owns the GPU copy knows to re-upload.
class Mesh {
  public:
	explicit Mesh(const std::string &debug_name);
	~Mesh() = default;

	AQUILA_NONCOPYABLE(Mesh);

	// Loading — CPU side only, no GPU involvement
	void load(const std::string &filepath);
	void load_from_data(const MeshData &mesh_data);

	// CPU-side transforms
	void center_mesh_at_origin();

	// Procedural generators
	static MeshData generate_cube(F32 size);
	static MeshData generate_sphere(F32 radius, Uint32 segments, Uint32 rings);
	static MeshData generate_cylinder(F32 radius, F32 height, Uint32 segments);
	static MeshData generate_plane(F32 width, F32 height, Uint32 width_segments, Uint32 height_segments);
	/// Primitive meshes by path, numbers are optional and fall back to the defaults.
	///
	/// @code
	/// Mesh::generate_from_path("procedural://cube/2");
	/// Mesh::generate_from_path("procedural://sphere/0.5/32/16");
	/// Mesh::generate_from_path("procedural://plane/10/10/4/4");
	/// @endcode
	/// @return nullopt for anything else
	[[nodiscard]] static Option<MeshData> generate_from_path(std::string_view path);

	/// Points into this mesh, use it before the mesh changes or dies.
	[[nodiscard]] RHI::GPUMeshDesc to_gpu_desc() const {
		RHI::GPUMeshDesc desc;
		desc.vertices = &m_vertices;
		desc.indices = m_has_index_buffer ? &m_indices : nullptr;
		desc.primitives = &m_primitives;
		desc.debug_name = m_debug_name;
		return desc;
	}

	// Getters
	[[nodiscard]] const std::string &get_path() const { return m_path; }
	[[nodiscard]] const std::string &get_debug_name() const { return m_debug_name; }
	[[nodiscard]] const std::vector<RHI::Vertex> &get_vertices() const { return m_vertices; }
	[[nodiscard]] const std::vector<Uint32> &get_indices() const { return m_indices; }
	[[nodiscard]] const std::vector<RHI::GPUMeshPrimitive> &get_primitives() const { return m_primitives; }
	[[nodiscard]] Uint32 get_vertex_count() const { return m_vertex_count; }
	[[nodiscard]] Uint32 get_index_count() const { return m_index_count; }
	[[nodiscard]] bool has_index_buffer() const { return m_has_index_buffer; }
	[[nodiscard]] Uint64 get_revision() const { return m_revision; }

  private:
	std::string m_debug_name;
	std::string m_path;

	std::vector<RHI::Vertex> m_vertices;
	std::vector<Uint32> m_indices;
	std::vector<RHI::GPUMeshPrimitive> m_primitives;

	Uint32 m_vertex_count = 0;
	Uint32 m_index_count = 0;
	bool m_has_index_buffer = false;
	Uint64 m_revision = 0;
};

} // namespace Aquila::Graphics::Resources
#endif
