#include "UI/Inspector/Common/AssetInspectable.h"

#include "Core/EditorContext.h"

namespace Editor {

std::string asset_icon(const std::string &path, EditorContext &context) {
	const AssetType *type = context.asset_types.find_for_path(path);
	return type != nullptr ? type->icon : std::string("file");
}

Aquila::UI::Core::Inspectable make_asset_inspectable(const std::string &path, EditorContext &context,
													 std::vector<Aquila::UI::Core::InspectorSection> sections) {
	const AssetType *type = context.asset_types.find_for_path(path);
	Aquila::UI::Core::Inspectable inspectable;
	inspectable.id = path;
	inspectable.icon = asset_icon(path, context);
	inspectable.title = std::filesystem::path(path).stem().string();
	inspectable.subtitle = path;
	inspectable.subtitle_tooltip = path;
	inspectable.tabs.emplace_back();
	inspectable.tabs.back().title = type != nullptr ? type->title : std::string("File");
	inspectable.tabs.back().sections = std::move(sections);
	return inspectable;
}

}
