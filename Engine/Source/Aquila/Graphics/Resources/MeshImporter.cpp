#include "Aquila/Graphics/Resources/MeshImporter.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

namespace Aquila::Graphics::Resources {

namespace {

constexpr Int64 k_full_processing_limit = 5 * 1024 * 1024;

void center_at_origin(std::vector<RHI::Vertex> &vertices) {
	if (vertices.empty()) {
		return;
	}
	Vec3 min_b = vertices.front().pos;
	Vec3 max_b = vertices.front().pos;
	for (const auto &v : vertices) {
		min_b = Math::min(min_b, v.pos);
		max_b = Math::max(max_b, v.pos);
	}
	const Vec3 center = (min_b + max_b) * 0.5F;
	for (auto &v : vertices) {
		v.pos -= center;
	}
}

Vec3 to_engine_space(const aiVector3D &v) {
	return { -v.x, v.y, v.z };
}

RHI::Vertex to_vertex(const aiMesh &mesh, Uint32 index) {
	RHI::Vertex v{};
	v.pos = to_engine_space(mesh.mVertices[index]);
	v.color = Vec3(1.F);
	v.normals = mesh.HasNormals() ? Math::normalize(to_engine_space(mesh.mNormals[index])) : Vec3(0, 1, 0);
	if (mesh.HasTextureCoords(0)) {
		v.texcoord = Vec2(mesh.mTextureCoords[0][index].x, 1.F - mesh.mTextureCoords[0][index].y);
	}

	if (mesh.HasTangentsAndBitangents() && mesh.HasNormals()) {
		const Vec3 t = Math::normalize(to_engine_space(mesh.mTangents[index]));
		const Vec3 b = Math::normalize(to_engine_space(mesh.mBitangents[index]));
		const F32 handedness = (Math::dot(Math::cross(v.normals, t), b) < 0.F) ? -1.F : 1.F;
		v.tangent = Vec4(t, handedness);
	} else {
		v.tangent = Vec4(1, 0, 0, -1);
	}
	return v;
}

void generate_tangents(MeshData &data, Uint32 first_vertex, Uint32 first_index) {
	const Usize count = data.vertices.size() - first_vertex;
	std::vector<Vec3> along_u(count, Vec3(0.F));
	std::vector<Vec3> image_up(count, Vec3(0.F));
	for (Usize i = first_index; i + 2 < data.indices.size(); i += 3) {
		const std::array<Uint32, 3> corners = { data.indices[i] - first_vertex, data.indices[i + 1] - first_vertex,
												data.indices[i + 2] - first_vertex };
		const RHI::Vertex &a = data.vertices[first_vertex + corners[0]];
		const RHI::Vertex &b = data.vertices[first_vertex + corners[1]];
		const RHI::Vertex &c = data.vertices[first_vertex + corners[2]];
		const Vec3 e1 = b.pos - a.pos;
		const Vec3 e2 = c.pos - a.pos;
		const Vec2 d1 = b.texcoord - a.texcoord;
		const Vec2 d2 = c.texcoord - a.texcoord;
		const F32 det = d1.x * d2.y - d2.x * d1.y;
		if (det == 0.F) {
			continue;
		}
		const F32 sign = det < 0.F ? -1.F : 1.F;
		for (Uint32 corner : corners) {
			along_u[corner] += (e1 * d2.y - e2 * d1.y) * sign;
			image_up[corner] += (e1 * d2.x - e2 * d1.x) * sign;
		}
	}
	for (Usize i = 0; i < count; ++i) {
		RHI::Vertex &v = data.vertices[first_vertex + i];
		const Vec3 t = along_u[i] - v.normals * Math::dot(v.normals, along_u[i]);
		const F32 length = Math::length(t);
		if (length <= 0.F) {
			continue;
		}
		v.tangent = Vec4(t / length, Math::dot(Math::cross(v.normals, t), image_up[i]) < 0.F ? -1.F : 1.F);
	}
}

void append_node(const aiScene &scene, const aiNode &node, MeshData &data) {
	for (Uint32 i = 0; i < node.mNumMeshes; i++) {
		const aiMesh &mesh = *scene.mMeshes[node.mMeshes[i]];
		const auto vertex_offset = static_cast<Uint32>(data.vertices.size());
		const auto index_offset = static_cast<Uint32>(data.indices.size());

		for (Uint32 j = 0; j < mesh.mNumVertices; j++) {
			data.vertices.push_back(to_vertex(mesh, j));
		}
		for (Uint32 j = 0; j < mesh.mNumFaces; j++) {
			const aiFace &face = mesh.mFaces[j];
			for (Uint32 k = face.mNumIndices; k > 0; k--) {
				data.indices.push_back(face.mIndices[k - 1] + vertex_offset);
			}
		}
		if (!mesh.HasTangentsAndBitangents() || !mesh.HasNormals()) {
			generate_tangents(data, vertex_offset, index_offset);
		}

		data.primitives.push_back({
			.first_index = index_offset,
			.first_vertex = vertex_offset,
			.index_count = static_cast<Uint32>(data.indices.size()) - index_offset,
			.vertex_count = mesh.mNumVertices,
		});
	}
	for (Uint32 i = 0; i < node.mNumChildren; i++) {
		append_node(scene, *node.mChildren[i], data);
	}
}

}

Option<MeshData> import_mesh(const std::string &path) {
	const auto file =
		Platform::Filesystem::VirtualFileSystem::get()->open_file(path, AccessMode::Read, OpenMode::Binary);
	if (!file || !file->is_valid()) {
		AQUILA_LOG_ERROR("MeshImporter: can't open {}", path);
		return std::nullopt;
	}

	const Int64 file_size = file->size();
	if (file_size <= 0) {
		AQUILA_LOG_ERROR("MeshImporter: {} is empty", path);
		return std::nullopt;
	}

	std::vector<Uint8> buffer(static_cast<Usize>(file_size));
	if (file->read(buffer.data(), buffer.size()) != buffer.size()) {
		AQUILA_LOG_ERROR("MeshImporter: couldn't read all of {}", path);
		return std::nullopt;
	}

	Uint32 flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality;
	if (file_size < k_full_processing_limit) {
		flags |= aiProcess_GenSmoothNormals | aiProcess_GenUVCoords;
	}

	Assimp::Importer importer;
	const std::string hint = std::filesystem::path(path).extension().string();
	const aiScene *scene =
		importer.ReadFileFromMemory(buffer.data(), buffer.size(), flags, hint.empty() ? "" : hint.c_str() + 1);
	if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0U || scene->mRootNode == nullptr) {
		AQUILA_LOG_ERROR("MeshImporter: assimp can't read {}: {}", path, importer.GetErrorString());
		return std::nullopt;
	}

	MeshData data;
	append_node(*scene, *scene->mRootNode, data);
	if (data.vertices.empty()) {
		AQUILA_LOG_ERROR("MeshImporter: {} has no geometry", path);
		return std::nullopt;
	}
	center_at_origin(data.vertices);

	AQUILA_LOG_INFO("MeshImporter: {} - {} vertices, {} indices, {} primitives", path, data.vertices.size(),
					data.indices.size(), data.primitives.size());
	return data;
}

}
