#ifndef AQUILA_PLAYER_BUILD_LAYOUT_H
#define AQUILA_PLAYER_BUILD_LAYOUT_H

namespace Aquila::Player {

struct BuildLayout {
	std::string root;

	[[nodiscard]] std::string executable(std::string_view name) const;
	[[nodiscard]] std::string manifest_file() const;
	[[nodiscard]] std::string engine_resources_dir() const;
	[[nodiscard]] std::string engine_shaders_dir() const;
	[[nodiscard]] std::string project_dir() const;
	/// The game's window icon, only there when the build picked one.
	[[nodiscard]] std::string icon_file() const;
};

[[nodiscard]] std::string player_template_dir(std::string_view editor_dir);

} // namespace Aquila::Player

#endif
