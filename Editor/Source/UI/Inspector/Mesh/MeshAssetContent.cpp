#include "UI/Inspector/Mesh/MeshAssetContent.h"

#include "Aquila/Graphics/Resources/MeshLibrary.h"
#include "Aquila/UI/Widgets/MessageBox.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "UI/Inspector/Common/AssetInspectable.h"
#include "UI/Inspector/Mesh/MeshStatsRows.h"

namespace Editor {

using namespace Aquila;

MeshAssetContent::MeshAssetContent(std::string path) : m_path(std::move(path)) {}

MeshAssetContent::~MeshAssetContent() = default;

void MeshAssetContent::build_below(UI::Core::PropertyGrid &grid) {
	m_mesh = Graphics::Resources::MeshLibrary::instance().load(Graphics::Resources::MeshSource::asset(m_path));
	if (m_mesh == nullptr) {
		UI::Core::create_message_box(grid, UI::Core::MessageKind::Error, "Doesn't import, see the console");
		return;
	}
	m_stats = std::make_unique<MeshStatsRows>(grid);
	m_stats->show(m_mesh.get());
}

Option<UI::Core::Inspectable> inspect_mesh_asset(const std::string &path, EditorContext &context) {
	UI::Core::InspectorSection model;
	model.id = "model";
	model.title = "Model";
	model.icon = asset_icon(path, context);
	model.content = std::make_unique<MeshAssetContent>(path);

	std::vector<UI::Core::InspectorSection> sections;
	sections.push_back(std::move(model));
	return make_asset_inspectable(path, context, std::move(sections));
}

}
