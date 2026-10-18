#ifndef AQUILA_GRAPHICS_RESOURCES_MESH_LIBRARY_H
#define AQUILA_GRAPHICS_RESOURCES_MESH_LIBRARY_H

#include "Aquila/Graphics/Resources/Mesh.h"
#include "Aquila/Graphics/Resources/MeshSource.h"

namespace Aquila::Graphics::Resources {

/// Shares one Mesh per source, so a hundred default cubes are one Mesh and one GPU upload.
/// Meshes are held weakly: one dies with its last user and the next load() builds it again.
/// Main thread only.
class MeshLibrary {
  public:
	static MeshLibrary &instance();

	/// Procedural params are resolved first, so the mesh's source lists all of them.
	/// @return null for a None source, an unknown generator or a model that fails to import (logged)
	[[nodiscard]] Ref<Mesh> load(const MeshSource &source);

	/// Forgets every mesh. The ones in use stay alive, later loads build new ones (after a model changed on disk).
	void clear();

  private:
	MeshLibrary() = default;

	std::unordered_map<std::string, std::weak_ptr<Mesh>> m_meshes;
};

}

#endif
