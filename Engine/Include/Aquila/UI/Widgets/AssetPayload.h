#pragma once

namespace Aquila::UI::Core {

/// What an asset drag carries. `asset_type` is like "texture" or "script", AssetSlot matches on it.
struct AssetPayload {
	std::string asset_path;
	std::string asset_type;
	std::string display_name;
};

} // namespace Aquila::UI::Core
