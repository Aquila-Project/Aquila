#ifndef AQUILA_PLAYER_BUILD_MANIFEST_H
#define AQUILA_PLAYER_BUILD_MANIFEST_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Rendering/RenderSettings.h"

namespace Aquila::Player {

struct BuildManifest {
	Uint32 version = 1;
	std::string name;
	std::string startup_scene;
	Uint32 window_width = 1280;
	Uint32 window_height = 720;
	/// The game's name when empty.
	std::string window_title;
	bool maximized = false;
	Rendering::ToneMapper tone_mapper = Rendering::ToneMapper::AgX;
	bool shadows = true;

	[[nodiscard]] bool is_valid() const {
		if (version > BuildManifest{}.version) {
			return false;
		}

		if (name.empty() || startup_scene.empty()) {
			return false;
		}

		constexpr std::string_view suffix = ".aqscene";
		return startup_scene.length() >= suffix.length() && startup_scene.ends_with(suffix);
	}
};

[[nodiscard]] std::string write_manifest(const BuildManifest &manifest);
/// @return nullopt for broken JSON, missing fields, wrong types or a newer version than this player knows
[[nodiscard]] Option<BuildManifest> read_manifest(std::string_view text);
/// read_manifest() on a native file, nullopt when it can't be read.
[[nodiscard]] Option<BuildManifest> load_manifest(const std::string &file);

} // namespace Aquila::Player

#endif
