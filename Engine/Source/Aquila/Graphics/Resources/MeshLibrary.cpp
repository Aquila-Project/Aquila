#include "Aquila/Graphics/Resources/MeshLibrary.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Graphics/Resources/MeshImporter.h"
#include "Aquila/Graphics/Resources/ProceduralMeshes.h"

namespace Aquila::Graphics::Resources {

MeshLibrary &MeshLibrary::instance() {
	static MeshLibrary library;
	return library;
}

Ref<Mesh> MeshLibrary::load(const MeshSource &source) {
	MeshSource resolved = source;
	Option<MeshData> data;

	switch (source.kind) {
	case MeshSourceKind::None:
		return nullptr;
	case MeshSourceKind::Procedural: {
		Option<MeshParams> params = MeshGenerators::instance().resolve(source.generator, source.params);
		if (!params) {
			AQUILA_LOG_ERROR("MeshLibrary: no procedural mesh called '{}'", source.generator);
			return nullptr;
		}
		resolved.params = std::move(*params);
		break;
	}
	case MeshSourceKind::Asset:
		break;
	}

	const std::string key = resolved.key();
	if (auto it = m_meshes.find(key); it != m_meshes.end()) {
		if (Ref<Mesh> cached = it->second.lock()) {
			return cached;
		}
	}

	data = resolved.kind == MeshSourceKind::Procedural ? MeshGenerators::instance().generate(resolved)
													   : import_mesh(resolved.path);
	if (!data) {
		return nullptr;
	}

	auto mesh = std::make_shared<Mesh>(resolved.describe());
	mesh->set_data(std::move(*data), std::move(resolved));

	std::erase_if(m_meshes, [](const auto &entry) { return entry.second.expired(); });
	m_meshes[key] = mesh;
	return mesh;
}

void MeshLibrary::clear() {
	m_meshes.clear();
}

}
