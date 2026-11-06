#pragma once

#include "Aquila/UI/Widgets/Inspectable.h"

namespace Editor {

class EditorContext;
class MaterialEditorRows;

/// Edits are saved to the file right away.
class MaterialAssetContent final : public Aquila::UI::Core::IInspectorContent {
  public:
	MaterialAssetContent(std::string path, EditorContext &context);
	~MaterialAssetContent() override;

	void build_below(Aquila::UI::Core::PropertyGrid &grid) override;
	void refresh() override;

  private:
	void show_material();

	std::string m_path;
	EditorContext &m_context;
	Unique<MaterialEditorRows> m_rows;
};

/// Nullopt when the file doesn't load.
[[nodiscard]] Option<Aquila::UI::Core::Inspectable> inspect_material_asset(const std::string &path,
																		   EditorContext &context);

}
