#ifndef AQUILA_GRAPHICS_RESOURCES_MESH_IMPORTER_H
#define AQUILA_GRAPHICS_RESOURCES_MESH_IMPORTER_H

#include "Aquila/Graphics/Resources/Mesh.h"

namespace Aquila::Graphics::Resources {

/// Reads a model file through the VFS with assimp. Every mesh in the file becomes one primitive and the whole
/// thing is centred on the origin. Blocks while it reads and parses.
/// @return nullopt when the file can't be read or parsed, the reason is logged
[[nodiscard]] Option<MeshData> import_mesh(const std::string &path);

}

#endif
