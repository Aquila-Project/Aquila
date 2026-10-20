#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include "Aquila/Foundation/PrimitiveTypes.h"
namespace Aquila::Platform::Filesystem {
struct FileStatInfo {
	Uint32 size = 0;
	bool exists = false;
	bool is_directory = false;
	bool is_regular_file = false;
	Uint32 last_write_time = 0;
};
std::string path_join(const std::string &a, const std::string &b);
std::string path_normalize(const std::string &path);
bool path_is_absolute(const std::string &path);
std::string dir_get_current();
bool dir_set_current(const std::string &path);
bool file_exists(const std::string &path);
FileStatInfo file_stat(const std::string &path);
bool dir_create(const std::string &path);
bool dir_remove(const std::string &path);
bool file_remove(const std::string &path);
bool file_move(const std::string &from, const std::string &to);
/// Names only, not full paths. With `recursive` nested names lose their folder.
std::vector<std::string> dir_list(const std::string &path, bool recursive = false);

// Helpers
std::string path_to_absolute(const std::string &path);
std::string path_extension(const std::string &path);
std::string path_executable_dir();
/// File name of the running executable without its folder or extension.
std::string path_executable_name();
/// Per-user folder for caches: $XDG_CACHE_HOME or ~/.cache, %LOCALAPPDATA% on Windows, ~/Library/Caches on macOS.
/// Falls back to the current directory when the environment has none of them.
std::string path_user_cache_dir();
} // namespace Aquila::Platform::Filesystem
#endif // FILESYSTEM_H
