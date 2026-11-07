#include "UI/Inspector/Mesh/MeshStatsRows.h"

#include "Aquila/Graphics/Resources/Mesh.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"

namespace Editor {

MeshStatsRows::MeshStatsRows(Aquila::UI::Core::PropertyGrid &grid)
	: m_vertices(grid.add_row<Aquila::UI::Core::Label>("Vertices", std::string("0"))),
	  m_triangles(grid.add_row<Aquila::UI::Core::Label>("Triangles", std::string("0"))) {}

void MeshStatsRows::show(const Aquila::Graphics::Resources::Mesh *mesh) {
	if (m_vertices != nullptr) {
		m_vertices->set_text(std::to_string(mesh != nullptr ? mesh->get_vertex_count() : 0));
	}
	if (m_triangles != nullptr) {
		m_triangles->set_text(std::to_string(mesh != nullptr ? mesh->get_index_count() / 3 : 0));
	}
}

}
