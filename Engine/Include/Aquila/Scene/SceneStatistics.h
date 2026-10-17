#ifndef AQUILA_SCENE_SCENE_STATISTICS_H
#define AQUILA_SCENE_SCENE_STATISTICS_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Scene/Entity.h"

namespace Aquila::Graphics::Resources {
class Mesh;
}

namespace Aquila::SceneManagement {

class Scene;

struct GeometryStatistics {
	Uint64 vertices = 0;
	Uint64 triangles = 0;
};

struct SceneStatistics {
	Uint32 objects = 0;
	Uint32 meshes = 0;
	Uint32 lights = 0;
	GeometryStatistics total;

	Uint32 selected_objects = 0;
	GeometryStatistics selected;
};

class StatisticsCollector {
  public:
	[[nodiscard]] SceneStatistics collect(Scene &scene, Entity selected);

  private:
	struct CachedMesh {
		Uint32 vertex_count = 0;
		Uint32 index_count = 0;
		GeometryStatistics counts;
	};

	[[nodiscard]] GeometryStatistics geometry_of(const Graphics::Resources::Mesh &mesh);

	std::unordered_map<const Graphics::Resources::Mesh *, CachedMesh> m_cache;
};

} // namespace Aquila::SceneManagement

#endif
