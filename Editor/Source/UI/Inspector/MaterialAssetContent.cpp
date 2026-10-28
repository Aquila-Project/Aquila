#include "UI/Inspector/MaterialAssetContent.h"

#include "Aquila/Graphics/Material/MaterialAssets.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Core/EditorContext.h"
#include "UI/Inspector/ThumbnailContent.h"
#include "UI/Inspector/MaterialEditorRows.h"

namespace Editor {

using Aquila::Graphics::MaterialAssets;

MaterialAssetContent::MaterialAssetContent(std::string path, EditorContext &context)
	: m_path(std::move(path)), m_context(context) {}

MaterialAssetContent::~MaterialAssetContent() = default;

void MaterialAssetContent::build_below(Aquila::UI::Core::PropertyGrid &grid) {
	m_rows = std::make_unique<MaterialEditorRows>(grid, &m_context.engine().get_context());
	show_material();
}

void MaterialAssetContent::refresh() {
	if (m_rows != nullptr) {
		m_rows->refresh();
	}
}

void MaterialAssetContent::show_material() {
	Aquila::GFX::GfxContext &gfx = m_context.engine().get_context();
	m_rows->show(MaterialAssets::get()->load(gfx, m_path),
				 [this, &gfx](const Aquila::Graphics::MaterialData &data, bool needs_rebuild) {
					 MaterialAssets::get()->edit(gfx, m_path, data, needs_rebuild);
					 if (needs_rebuild) {
						 show_material();
					 }
				 });
}

Option<Aquila::UI::Core::Inspectable> inspect_material_asset(const std::string &path, EditorContext &context) {
	if (MaterialAssets::get()->load(context.engine().get_context(), path) == nullptr) {
		return std::nullopt;
	}

	Aquila::UI::Core::InspectorSection preview;
	preview.id = "preview";
	preview.content = std::make_unique<ThumbnailContent>(path, context);

	Aquila::UI::Core::InspectorSection material;
	material.id = "material";
	material.title = "Material";
	material.icon = "eclipse";
	material.content = std::make_unique<MaterialAssetContent>(path, context);

	Aquila::UI::Core::Inspectable inspectable;
	inspectable.id = path;
	inspectable.icon = "eclipse";
	inspectable.title = std::filesystem::path(path).stem().string();
	inspectable.subtitle = path;
	inspectable.subtitle_tooltip = path;
	inspectable.tabs.emplace_back();
	inspectable.tabs.back().title = "Material";
	inspectable.tabs.back().sections.push_back(std::move(preview));
	inspectable.tabs.back().sections.push_back(std::move(material));
	return inspectable;
}

}
