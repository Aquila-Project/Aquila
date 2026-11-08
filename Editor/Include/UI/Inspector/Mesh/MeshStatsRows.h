#pragma once

namespace Aquila::Graphics::Resources {
class Mesh;
}

namespace Aquila::UI::Core {
class Label;
class PropertyGrid;
}

namespace Editor {

/// Vertices and Triangles rows. Shared by MeshContent and MeshAssetContent.
class MeshStatsRows {
  public:
	explicit MeshStatsRows(Aquila::UI::Core::PropertyGrid &grid);

	/// Null shows zeros.
	void show(const Aquila::Graphics::Resources::Mesh *mesh);

  private:
	Aquila::UI::Core::Label *m_vertices = nullptr;
	Aquila::UI::Core::Label *m_triangles = nullptr;
};

}
