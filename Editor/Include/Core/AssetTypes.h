#pragma once

#include "Aquila/UI/Widgets/Inspectable.h"

namespace Editor {

class EditorContext;

enum class AssetThumbnail : Uint8 { None, Image, Material };

struct AssetType {
	/// Also the asset slot and drag type: "texture", "material", "model".
	std::string id;
	std::string title;
	std::string icon = "file";
	/// Lower case with the dot: ".png".
	std::vector<std::string> extensions;
	AssetThumbnail thumbnail = AssetThumbnail::None;
	/// Content browser double click.
	Delegate<void(const std::string &path)> open{};
	/// Nullopt when the file can't be shown. Not set: clicking the file leaves the inspector alone.
	Delegate<Option<Aquila::UI::Core::Inspectable>(const std::string &path, EditorContext &context)> inspect{};
};

/// Content browser, thumbnails, asset slots and the inspector all read this.
class AssetTypes {
  public:
	/// Same id replaces the old one.
	void add(AssetType type);

	[[nodiscard]] const AssetType *find(std::string_view id) const;
	/// Longest matching extension wins (".d.luau" over ".luau").
	[[nodiscard]] const AssetType *find_for_path(std::string_view path) const;
	[[nodiscard]] const std::vector<AssetType> &get_all() const { return m_types; }

  private:
	std::vector<AssetType> m_types;
};

void add_builtin_asset_types(AssetTypes &types);

}
