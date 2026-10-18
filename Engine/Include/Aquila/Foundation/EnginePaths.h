#ifndef AQUILA_FOUNDATION_ENGINE_PATHS_H
#define AQUILA_FOUNDATION_ENGINE_PATHS_H

namespace Aquila::Foundation::EnginePaths {

/// Native folders the engine reads its own shaders and resources from, always ending in a slash.
/// They start as the source tree folders CMake baked in, a shipped player points them at its Data folder.
/// @note Main thread only. Set them before the Application exists: render systems resolve their shaders when
/// they build pipelines and the VFS mounts /resources and /shaders in init_rendering().
void set_shaders_dir(std::string_view directory);
void set_resources_dir(std::string_view directory);

[[nodiscard]] const std::string &shaders_dir();
[[nodiscard]] const std::string &resources_dir();

} // namespace Aquila::Foundation::EnginePaths

#endif
