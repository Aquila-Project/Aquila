#pragma once

#include "Aquila/UI/Widgets/Inspectable.h"

namespace Aquila::Graphics::Resources {
class Mesh;
}

namespace Editor {

class EditorContext;
class MeshStatsRows;

/// Imports the model when the section is built (MeshLibrary shares one that's already loaded), not when the file
/// changes on disk. Read only.
class MeshAssetContent final : public Aquila::UI::Core::IInspectorContent {
  public:
	explicit MeshAssetContent(std::string path);
	~MeshAssetContent() override;

	void build_below(Aquila::UI::Core::PropertyGrid &grid) override;

  private:
	std::string m_path;
	Ref<Aquila::Graphics::Resources::Mesh> m_mesh;
	Unique<MeshStatsRows> m_stats;
};

[[nodiscard]] Option<Aquila::UI::Core::Inspectable> inspect_mesh_asset(const std::string &path, EditorContext &context);

}
