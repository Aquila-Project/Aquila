#include "Aquila/Graphics/Resources/Mesh.h"

#include "Aquila/Foundation/Math/Math.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include <charconv>

namespace Aquila::Graphics::Resources {

Mesh::Mesh(const std::string &debug_name) : m_debug_name(debug_name) {}

Option<MeshData> Mesh::generate_from_path(std::string_view path) {
	constexpr std::string_view k_scheme = "procedural://";
	if (!path.starts_with(k_scheme)) {
		return std::nullopt;
	}
	path.remove_prefix(k_scheme.size());

	std::vector<std::string_view> parts;
	for (Usize start = 0; start <= path.size();) {
		const Usize end = Math::min(path.find('/', start), path.size());
		parts.push_back(path.substr(start, end - start));
		start = end + 1;
	}

	std::vector<F32> numbers;
	for (Usize i = 1; i < parts.size(); ++i) {
		F32 value = 0.F;
		const auto [ptr, error] = std::from_chars(parts[i].data(), parts[i].data() + parts[i].size(), value);
		if (error != std::errc{}) {
			return std::nullopt;
		}
		numbers.push_back(value);
	}

	auto number = [&numbers](Usize index, F32 fallback) { return index < numbers.size() ? numbers[index] : fallback; };
	auto count = [&](Usize index, Uint32 fallback) {
		return static_cast<Uint32>(Math::max(number(index, static_cast<F32>(fallback)), 1.F));
	};

	const std::string_view kind = parts.front();
	if (kind == "cube") {
		return generate_cube(number(0, 1.F));
	}
	if (kind == "sphere") {
		return generate_sphere(number(0, 0.5F), count(1, 32), count(2, 16));
	}
	if (kind == "cylinder") {
		return generate_cylinder(number(0, 0.5F), number(1, 1.F), count(2, 32));
	}
	if (kind == "plane") {
		return generate_plane(number(0, 1.F), number(1, 1.F), count(2, 1), count(3, 1));
	}
	return std::nullopt;
}

void Mesh::load(const std::string &filepath) {
	Assimp::Importer importer;

	const auto file =
		Platform::Filesystem::VirtualFileSystem::get()->open_file(filepath, AccessMode::Read, OpenMode::Binary);
	if (!file || !file->is_valid()) {
		throw std::runtime_error("Failed to open mesh via VFS: " + filepath);
	}

	const Int64 file_size = file->size();
	if (file_size <= 0) {
		throw std::runtime_error("Mesh file is empty: " + filepath);
	}

	std::vector<Uint8> buffer(file_size);
	if (const size_t bytes_read = file->read(buffer.data(), buffer.size()); bytes_read != buffer.size()) {
		throw std::runtime_error("Failed to read entire mesh buffer from VFS");
	}

	Uint32 flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality;
	if (file_size < 5 * 1024 * 1024) {
		flags |= aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_GenUVCoords;
	}

	const aiScene *scene = importer.ReadFileFromMemory(buffer.data(), buffer.size(), flags);
	if ((scene == nullptr) || ((scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0u) || (scene->mRootNode == nullptr)) {
		throw std::runtime_error("Assimp failed: " + std::string(importer.GetErrorString()));
	}

	m_path = filepath;
	m_vertices.clear();
	m_indices.clear();
	m_primitives.clear();

	size_t total_vertices = 0, total_indices = 0;
	Delegate<void(aiNode *)> count = [&](aiNode *node) {
		for (Uint32 i = 0; i < node->mNumMeshes; i++) {
			aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
			total_vertices += mesh->mNumVertices;
			total_indices += mesh->mNumFaces * 3;
		}
		for (Uint32 i = 0; i < node->mNumChildren; i++) {
			count(node->mChildren[i]);
		}
	};
	count(scene->mRootNode);

	m_vertices.reserve(total_vertices);
	m_indices.reserve(total_indices);

	Delegate<void(aiNode *, const aiScene *)> process_node;
	process_node = [&](aiNode *node, const aiScene *s) {
		for (Uint32 i = 0; i < node->mNumMeshes; i++) {
			aiMesh *mesh = s->mMeshes[node->mMeshes[i]];

			Uint32 vertex_offset = static_cast<Uint32>(m_vertices.size());
			Uint32 index_offset = static_cast<Uint32>(m_indices.size());

			for (Uint32 j = 0; j < mesh->mNumVertices; j++) {
				RHI::Vertex v{};
				v.pos = Vec4(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z, 1.F);
				v.normals = mesh->HasNormals()
					? Math::normalize(Vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z))
					: Vec3(0, 1, 0);
				v.texcoord = mesh->HasTextureCoords(0)
					? Vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y)
					: Vec2(0);

				if (mesh->HasTangentsAndBitangents()) {
					Vec3 t = Math::normalize(Vec3(mesh->mTangents[j].x, mesh->mTangents[j].y, mesh->mTangents[j].z));
					Vec3 b =
						Math::normalize(Vec3(mesh->mBitangents[j].x, mesh->mBitangents[j].y, mesh->mBitangents[j].z));
					Vec3 n = Math::normalize(Vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z));
					F32 h = (Math::dot(Math::cross(n, t), b) < 0.F) ? -1.F : 1.F;
					v.tangent = Vec4(t, h);
				} else {
					v.tangent = Vec4(1, 0, 0, 1);
				}
				m_vertices.push_back(v);
			}

			for (Uint32 j = 0; j < mesh->mNumFaces; j++) {
				const aiFace &face = mesh->mFaces[j];
				for (Uint32 k = 0; k < face.mNumIndices; k++) {
					m_indices.push_back(face.mIndices[k] + vertex_offset);
				}
			}

			RHI::GPUMeshPrimitive prim{};
			prim.first_vertex = vertex_offset;
			prim.vertex_count = mesh->mNumVertices;
			prim.first_index = index_offset;
			prim.index_count = mesh->mNumFaces * 3;
			m_primitives.push_back(prim);
		}
		for (Uint32 i = 0; i < node->mNumChildren; i++) {
			process_node(node->mChildren[i], s);
		}
	};
	process_node(scene->mRootNode, scene);

	center_mesh_at_origin();

	m_vertex_count = static_cast<Uint32>(m_vertices.size());
	m_index_count = static_cast<Uint32>(m_indices.size());
	m_has_index_buffer = m_index_count > 0;
	++m_revision;

	AQUILA_LOG_INFO("Loaded mesh '{}' - {} vertices, {} indices", m_debug_name, m_vertex_count, m_index_count);
}

void Mesh::load_from_data(const MeshData &mesh_data) {
	if (mesh_data.vertices.empty()) {
		AQUILA_LOG_ERROR("Mesh data has no vertices: {}", m_debug_name);
		return;
	}

	m_vertices = mesh_data.vertices;
	m_indices = mesh_data.indices;
	m_path = mesh_data.path;

	m_vertex_count = static_cast<Uint32>(m_vertices.size());
	m_index_count = static_cast<Uint32>(m_indices.size());
	m_has_index_buffer = m_index_count > 0;

	RHI::GPUMeshPrimitive prim{};
	prim.first_vertex = 0;
	prim.vertex_count = m_vertex_count;
	prim.first_index = 0;
	prim.index_count = m_index_count;
	m_primitives = { prim };
	++m_revision;

	AQUILA_LOG_INFO("Loaded mesh '{}' - {} vertices, {} indices", m_debug_name, m_vertex_count, m_index_count);
}

void Mesh::center_mesh_at_origin() {
	if (m_vertices.empty()) {
		return;
	}

	Vec3 min_b = Vec3(m_vertices[0].pos);
	Vec3 max_b = Vec3(m_vertices[0].pos);
	for (const auto &v : m_vertices) {
		Vec3 p = Vec3(v.pos);
		min_b = Math::min(min_b, p);
		max_b = Math::max(max_b, p);
	}
	Vec3 center = (min_b + max_b) * 0.5F;
	for (auto &v : m_vertices) {
		v.pos = Vec4(Vec3(v.pos) - center, 1.F);
	}
	++m_revision;

	AQUILA_LOG_INFO("Centered mesh '{}' by ({}, {}, {})", m_debug_name, center.x, center.y, center.z);
}

MeshData Mesh::generate_cube(F32 size) {
	MeshData data;
	data.path = std::format("procedural://cube/{}", size);
	constexpr Vec3 white = { 1, 1, 1 };

	data.vertices = {
		{ { -size, -size, size }, white, { 0, 0, 1 }, { 0, 0 }, { 1, 0, 0, 1 } },
		{ { size, -size, size }, white, { 0, 0, 1 }, { 1, 0 }, { 1, 0, 0, 1 } },
		{ { size, size, size }, white, { 0, 0, 1 }, { 1, 1 }, { 1, 0, 0, 1 } },
		{ { -size, size, size }, white, { 0, 0, 1 }, { 0, 1 }, { 1, 0, 0, 1 } },
		{ { size, -size, -size }, white, { 0, 0, -1 }, { 0, 0 }, { -1, 0, 0, 1 } },
		{ { -size, -size, -size }, white, { 0, 0, -1 }, { 1, 0 }, { -1, 0, 0, 1 } },
		{ { -size, size, -size }, white, { 0, 0, -1 }, { 1, 1 }, { -1, 0, 0, 1 } },
		{ { size, size, -size }, white, { 0, 0, -1 }, { 0, 1 }, { -1, 0, 0, 1 } },
		{ { -size, -size, -size }, white, { -1, 0, 0 }, { 0, 0 }, { 0, 0, 1, 1 } },
		{ { -size, -size, size }, white, { -1, 0, 0 }, { 1, 0 }, { 0, 0, 1, 1 } },
		{ { -size, size, size }, white, { -1, 0, 0 }, { 1, 1 }, { 0, 0, 1, 1 } },
		{ { -size, size, -size }, white, { -1, 0, 0 }, { 0, 1 }, { 0, 0, 1, 1 } },
		{ { size, -size, size }, white, { 1, 0, 0 }, { 0, 0 }, { 0, 0, -1, 1 } },
		{ { size, -size, -size }, white, { 1, 0, 0 }, { 1, 0 }, { 0, 0, -1, 1 } },
		{ { size, size, -size }, white, { 1, 0, 0 }, { 1, 1 }, { 0, 0, -1, 1 } },
		{ { size, size, size }, white, { 1, 0, 0 }, { 0, 1 }, { 0, 0, -1, 1 } },
		{ { -size, size, size }, white, { 0, 1, 0 }, { 0, 0 }, { 1, 0, 0, 1 } },
		{ { size, size, size }, white, { 0, 1, 0 }, { 1, 0 }, { 1, 0, 0, 1 } },
		{ { size, size, -size }, white, { 0, 1, 0 }, { 1, 1 }, { 1, 0, 0, 1 } },
		{ { -size, size, -size }, white, { 0, 1, 0 }, { 0, 1 }, { 1, 0, 0, 1 } },
		{ { -size, -size, -size }, white, { 0, -1, 0 }, { 0, 0 }, { 1, 0, 0, 1 } },
		{ { size, -size, -size }, white, { 0, -1, 0 }, { 1, 0 }, { 1, 0, 0, 1 } },
		{ { size, -size, size }, white, { 0, -1, 0 }, { 1, 1 }, { 1, 0, 0, 1 } },
		{ { -size, -size, size }, white, { 0, -1, 0 }, { 0, 1 }, { 1, 0, 0, 1 } },
	};
	data.indices = {
		0,	1,	2,	2,	3,	0,	4,	5,	6,	6,	7,	4,	8,	9,	10, 10, 11, 8,
		12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20,
	};
	return data;
}

MeshData Mesh::generate_sphere(F32 radius, Uint32 segments, Uint32 rings) {
	MeshData data;
	data.path = std::format("procedural://sphere/{}/{}/{}", radius, segments, rings);
	constexpr Vec3 white = { 1, 1, 1 };

	data.vertices.push_back({ { 0, radius, 0 }, white, { 0, 1, 0 }, { 0.5F, 1.F }, { 1, 0, 0, 1 } });
	for (Uint32 stack = 1; stack < rings; ++stack) {
		F32 phi = Math::PI * F32(stack) / F32(rings);
		for (Uint32 slice = 0; slice < segments; ++slice) {
			F32 theta = Math::TAU * F32(slice) / F32(segments);
			Vec3 pos = { radius * Math::sin(phi) * Math::cos(theta), radius * Math::cos(phi), radius * Math::sin(phi) * Math::sin(theta) };
			data.vertices.push_back({ pos,
									  white,
									  Math::normalize(pos),
									  { F32(slice) / F32(segments), 1.F - F32(stack) / F32(rings) },
									  Math::normalize(Vec4(-Math::sin(theta), 0, Math::cos(theta), 1)) });
		}
	}
	data.vertices.push_back({ { 0, -radius, 0 }, white, { 0, -1, 0 }, { 0.5F, 0.F }, { 1, 0, 0, 1 } });

	Uint32 top = 0;
	Uint32 bottom = static_cast<Uint32>(data.vertices.size() - 1);

	for (Uint32 i = 0; i < segments; ++i) {
		data.indices.push_back(top);
		data.indices.push_back(1 + (i + 1) % segments);
		data.indices.push_back(1 + i);
	}
	Uint32 last_ring = 1 + (rings - 2) * segments;
	for (Uint32 i = 0; i < segments; ++i) {
		data.indices.push_back(bottom);
		data.indices.push_back(last_ring + i);
		data.indices.push_back(last_ring + (i + 1) % segments);
	}
	for (Uint32 stack = 0; stack < rings - 2; ++stack) {
		Uint32 curr = 1 + stack * segments;
		Uint32 next = curr + segments;
		for (Uint32 slice = 0; slice < segments; ++slice) {
			Uint32 i0 = curr + slice, i1 = curr + (slice + 1) % segments;
			Uint32 i2 = next + (slice + 1) % segments, i3 = next + slice;
			data.indices.insert(data.indices.end(), { i0, i1, i2, i0, i2, i3 });
		}
	}
	return data;
}

MeshData Mesh::generate_cylinder(F32 radius, F32 height, Uint32 segments) {
	MeshData data;
	data.path = std::format("procedural://cylinder/{}/{}/{}", radius, height, segments);
	constexpr Vec3 white = { 1, 1, 1 };
	F32 half = height * 0.5F;

	data.vertices.push_back({ { 0, half, 0 }, white, { 0, 1, 0 }, { 0.5F, 0.5F }, { 1, 0, 0, 1 } });
	data.vertices.push_back({ { 0, -half, 0 }, white, { 0, -1, 0 }, { 0.5F, 0.5F }, { 1, 0, 0, 1 } });

	constexpr Uint32 side_start = 2;
	for (Uint32 i = 0; i <= segments; i++) {
		F32 angle = Math::TAU * i / segments;
		F32 x = radius * Math::cos(angle), z = radius * Math::sin(angle);
		Vec3 n = Math::normalize(Vec3(x, 0, z));
		Vec4 t = Math::normalize(Vec4(-Math::sin(angle), 0, Math::cos(angle), 1));
		F32 u = F32(i) / segments;
		data.vertices.push_back({ { x, half, z }, white, n, { u, 0 }, t });
		data.vertices.push_back({ { x, -half, z }, white, n, { u, 1 }, t });
	}
	for (Uint32 i = 0; i < segments; i++) {
		Uint32 tc = side_start + i * 2, bc = tc + 1, tn = side_start + (i + 1) * 2, bn = tn + 1;
		data.indices.insert(data.indices.end(), { tc, tn, bc, tn, bn, bc });
	}
	for (Uint32 i = 0; i < segments; i++) {
		Uint32 curr = side_start + i * 2, next = side_start + ((i + 1) % segments) * 2;
		data.indices.insert(data.indices.end(), { 0u, next, curr });
	}
	for (Uint32 i = 0; i < segments; i++) {
		Uint32 curr = side_start + i * 2 + 1, next = side_start + ((i + 1) % segments) * 2 + 1;
		data.indices.insert(data.indices.end(), { 1u, curr, next });
	}
	return data;
}

MeshData Mesh::generate_plane(F32 width, F32 height, Uint32 w_segs, Uint32 h_segs) {
	MeshData data;
	data.path = std::format("procedural://plane/{}/{}/{}/{}", width, height, w_segs, h_segs);
	constexpr Vec3 white = { 1, 1, 1 };

	for (Uint32 y = 0; y <= h_segs; y++) {
		for (Uint32 x = 0; x <= w_segs; x++) {
			F32 u = F32(x) / w_segs, v = F32(y) / h_segs;
			data.vertices.push_back(
				{ { (u - 0.5F) * width, 0, (v - 0.5F) * height }, white, { 0, 1, 0 }, { u, v }, { 1, 0, 0, 1 } });
		}
	}
	for (Uint32 y = 0; y < h_segs; y++) {
		for (Uint32 x = 0; x < w_segs; x++) {
			Uint32 tl = y * (w_segs + 1) + x, tr = tl + 1;
			Uint32 bl = (y + 1) * (w_segs + 1) + x, br = bl + 1;
			data.indices.insert(data.indices.end(), { tl, bl, tr, tr, bl, br });
		}
	}
	return data;
}

} // namespace Aquila::Graphics::Resources
