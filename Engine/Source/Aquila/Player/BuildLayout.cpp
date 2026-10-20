#include "Aquila/Player/BuildLayout.h"

namespace Aquila::Player {

std::string BuildLayout::executable(std::string_view name) const {
	std::filesystem::path p = std::filesystem::path(root) / name;
	p += AQUILA_PLAYER_EXECUTABLE_SUFFIX;
	return p.string();
}

std::string BuildLayout::manifest_file() const {
	return (std::filesystem::path(root) / AQUILA_PLAYER_MANIFEST).string();
}

std::string BuildLayout::engine_resources_dir() const {
	return (std::filesystem::path(root) / AQUILA_PLAYER_ENGINE_RESOURCES_DIR).string();
}

std::string BuildLayout::engine_shaders_dir() const {
	return (std::filesystem::path(root) / AQUILA_PLAYER_ENGINE_SHADERS_DIR).string();
}

std::string BuildLayout::project_dir() const {
	return (std::filesystem::path(root) / AQUILA_PLAYER_PROJECT_DIR).string();
}

std::string BuildLayout::icon_file() const {
	return (std::filesystem::path(root) / AQUILA_PLAYER_ICON).string();
}

std::string player_template_dir(std::string_view editor_dir) {
	return (std::filesystem::path(editor_dir) / AQUILA_PLAYER_TEMPLATE_DIR).string();
}

} // namespace Aquila::Player