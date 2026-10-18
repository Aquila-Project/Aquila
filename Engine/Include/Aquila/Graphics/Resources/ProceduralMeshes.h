#ifndef AQUILA_GRAPHICS_RESOURCES_PROCEDURAL_MESHES_H
#define AQUILA_GRAPHICS_RESOURCES_PROCEDURAL_MESHES_H

#include "Aquila/Foundation/Reflection/TypeInfo.h"
#include "Aquila/Graphics/Resources/Mesh.h"
#include "Aquila/Graphics/Resources/MeshSource.h"

namespace Aquila::Graphics::Resources {

/// One number a generator takes. `value` is the default, edits are clamped to [min, max].
struct MeshParam {
	std::string name;
	std::string label;
	F32 value = 0.F;
	F32 min = 0.F;
	F32 max = 1000.F;
	bool integer = false;
	std::string unit;
};

/// A kind of procedural mesh. `generate` always gets every param, resolved and clamped.
struct MeshGenerator {
	std::string id;
	std::string title;
	std::vector<MeshParam> params;
	Delegate<MeshData(const MeshParams &)> generate;
};

/// Every procedural mesh the engine can build. Cube, sphere, cylinder and plane are there on first use,
/// plugins add their own with add() and get the inspector and saving for free.
/// Main thread only.
///
/// @code
/// MeshGenerators::instance().add({
///     .id = "torus",
///     .title = "Torus",
///     .params = { { .name = "radius", .label = "Radius", .value = 0.5F, .min = 0.01F, .max = 10.F,
///                   .integer = false, .unit = "m" } },
///     .generate = [](const MeshParams &params) { return make_torus(params.at("radius")); },
/// });
/// @endcode
class MeshGenerators {
  public:
	static MeshGenerators &instance();

	/// Replaces the generator with the same id.
	void add(MeshGenerator generator);

	[[nodiscard]] const MeshGenerator *find(std::string_view id) const;
	/// In the order they were added.
	[[nodiscard]] std::vector<const MeshGenerator *> all() const;

	/// Every param of the generator: missing ones get the default, all are clamped, unknown ones dropped.
	/// @return nullopt for an unknown generator
	[[nodiscard]] Option<MeshParams> resolve(std::string_view id, const MeshParams &params) const;
	/// nullopt for an unknown generator or a source that isn't procedural.
	[[nodiscard]] Option<MeshData> generate(const MeshSource &source) const;

	/// Reflection over a MeshParams for ReflectedPropertyGrid, one property per param. Null for unknown ids.
	/// @note Lives as long as the generator, add() with the same id destroys it.
	[[nodiscard]] const Reflection::TypeInfo *get_type(std::string_view id) const;

  private:
	MeshGenerators();

	struct Entry {
		MeshGenerator generator;
		Reflection::TypeInfo type{ "MeshParams" };
	};

	std::vector<Unique<Entry>> m_entries;
};

}

#endif
