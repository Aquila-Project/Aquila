#ifndef AQUILA_GRAPHICS_RESOURCES_MESH_SOURCE_H
#define AQUILA_GRAPHICS_RESOURCES_MESH_SOURCE_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Graphics::Resources {

/// Generator parameters by name. Integer parameters are stored as whole floats.
using MeshParams = std::map<std::string, F32, std::less<>>;

enum class MeshSourceKind : Uint8 { None, Procedural, Asset };

/// Where a mesh comes from. Scenes save this, never the vertices, and MeshLibrary shares one Mesh per source.
/// None is a mesh built in code: it renders but isn't saved.
struct MeshSource {
	MeshSourceKind kind = MeshSourceKind::None;
	/// Procedural only: a MeshGenerators id like "sphere". Missing params use the generator's defaults.
	std::string generator;
	MeshParams params;
	/// Asset only: VFS path of the model file, like "/project/Content/statue.glb". Becomes an asset id once there
	/// is an asset database.
	std::string path;

	[[nodiscard]] static MeshSource procedural(std::string generator, MeshParams params = {});
	[[nodiscard]] static MeshSource asset(std::string path);

	/// Reads the mesh paths scenes saved before MeshSource: "procedural://sphere/0.5/32/16" or a model path.
	/// Old cube numbers were half sizes, they are doubled.
	/// @return nullopt for an empty path or an unknown procedural kind
	[[nodiscard]] static Option<MeshSource> from_legacy_path(std::string_view path);

	/// Same string for the same mesh, MeshLibrary's cache key. Only stable for resolved procedural params.
	[[nodiscard]] std::string key() const;
	/// "sphere", "statue.glb", for logs and debug names.
	[[nodiscard]] std::string describe() const;

	bool operator==(const MeshSource &) const = default;
};

}

#endif
