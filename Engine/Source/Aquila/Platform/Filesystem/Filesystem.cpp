#include "Aquila/Platform/Filesystem/Filesystem.h"
#include "Aquila/Foundation/Macros.h"

#ifdef AQUILA_PLATFORM_LINUX
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <climits>
#endif

namespace Aquila::Platform::Filesystem {
using std::string;

bool file_exists(const std::string &path) {
#ifdef AQUILA_PLATFORM_WINDOWS
	const DWORD attr = GetFileAttributesA(path.c_str());
	return (attr != INVALID_FILE_ATTRIBUTES);
#elif defined(AQUILA_PLATFORM_LINUX) || defined(AQUILA_PLATFORM_MACOS)
	struct ::stat buffer;
	return (::stat(path.c_str(), &buffer) == 0);
#endif
}

FileStatInfo file_stat(const std::string &path) {
	FileStatInfo result;
#ifdef AQUILA_PLATFORM_WINDOWS
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data) != 0) {
		result.exists = true;
		result.size = (static_cast<Uint64>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
		result.is_directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		result.is_regular_file = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;

		ULARGE_INTEGER ull;
		ull.LowPart = data.ftLastWriteTime.dwLowDateTime;
		ull.HighPart = data.ftLastWriteTime.dwHighDateTime;
		result.last_write_time = static_cast<Uint32>((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
	}
#elif defined(AQUILA_PLATFORM_LINUX) || defined(AQUILA_PLATFORM_MACOS)
	struct ::stat buffer;
	if (::stat(path.c_str(), &buffer) == 0) {
		result.exists = true;
		result.size = static_cast<Uint32>(buffer.st_size);
		result.is_directory = S_ISDIR(buffer.st_mode);
		result.is_regular_file = S_ISREG(buffer.st_mode);
		result.last_write_time = static_cast<Uint32>(buffer.st_mtime);
	}
#endif
	return result;
}

bool path_is_absolute(const std::string &path) {
#ifdef AQUILA_PLATFORM_WINDOWS
	return (path.size() > 1 && path[1] == ':');
#elif defined(AQUILA_PLATFORM_LINUX) || defined(AQUILA_PLATFORM_MACOS)
	return (!path.empty() && path[0] == '/');
#endif
}

std::string path_join(const std::string &a, const std::string &b) {
	if (a.empty()) {
		return b;
	}
	if (b.empty()) {
		return a;
	}

	// Strip leading separator from b to avoid double separators
	const std::string &b_clean = (b[0] == '/' || b[0] == '\\') ? b.substr(1) : b;

	const bool trailed = (a.back() == '/' || a.back() == '\\');
	return trailed ? a + b_clean : a + '/' + b_clean;
}

std::string path_normalize(const std::string &path) {
	std::string fwd = path;
	std::ranges::replace(fwd, '\\', '/');

	// Collapse double slashes, resolve . and ..
	std::vector<std::string> parts;
	std::istringstream ss(fwd);
	std::string token;
	const bool absolute = (!fwd.empty() && fwd[0] == '/');
	while (std::getline(ss, token, '/')) {
		if (token.empty() || token == ".") {
			continue;
		}
		if (token == "..") {
			if (!parts.empty()) {
				parts.pop_back();
			}
		} else {
			parts.push_back(token);
		}
	}
	std::string out = absolute ? "/" : "";
	for (size_t i = 0; i < parts.size(); ++i) {
		out += parts[i];
		if (i + 1 < parts.size()) {
			out += '/';
		}
	}
	return out.empty() ? (absolute ? "/" : ".") : out;
}

std::string dir_get_current() {
#ifdef AQUILA_PLATFORM_WINDOWS
	wchar_t buffer[MAX_PATH];
	if (GetCurrentDirectoryW(MAX_PATH, buffer) != 0u) {
		std::wstring wstr(buffer);
		return path_normalize({ wstr.begin(), wstr.end() });
	}
	return {};
#elif defined(AQUILA_PLATFORM_LINUX) || defined(AQUILA_PLATFORM_MACOS)
	char buffer[PATH_MAX];
	if (getcwd(buffer, sizeof(buffer)) != nullptr) {
		return std::string(buffer);
	}
	return {};
#endif
}

std::string path_executable_dir() {
	std::string full;
#ifdef AQUILA_PLATFORM_WINDOWS
	char buffer[MAX_PATH] = {};
	const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
	if (length == 0 || length >= MAX_PATH) {
		return dir_get_current();
	}
	full.assign(buffer, length);
#elif defined(AQUILA_PLATFORM_LINUX) || defined(AQUILA_PLATFORM_MACOS)
	char buffer[PATH_MAX] = {};
	const ssize_t length = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
	if (length <= 0) {
		return dir_get_current();
	}
	full.assign(buffer, static_cast<std::size_t>(length));
#endif
	const std::string normalized = path_normalize(full);
	const std::size_t slash = normalized.find_last_of('/');
	return (slash == std::string::npos) ? normalized : normalized.substr(0, slash);
}

std::string path_executable_name() {
	std::string full;
#ifdef AQUILA_PLATFORM_WINDOWS
	char buffer[MAX_PATH] = {};
	const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
	if (length == 0 || length >= MAX_PATH) {
		return "Aquila";
	}
	full.assign(buffer, length);
#elif defined(AQUILA_PLATFORM_LINUX) || defined(AQUILA_PLATFORM_MACOS)
	char buffer[PATH_MAX] = {};
	const ssize_t length = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
	if (length <= 0) {
		return "Aquila";
	}
	full.assign(buffer, static_cast<std::size_t>(length));
#endif
	return std::filesystem::path(full).stem().string();
}

std::string path_user_cache_dir() {
	auto from_env = [](const char *name) -> std::string {
		const char *value = std::getenv(name);
		return (value != nullptr && value[0] != '\0') ? path_normalize(value) : std::string();
	};
#ifdef AQUILA_PLATFORM_WINDOWS
	std::string directory = from_env("LOCALAPPDATA");
#elif defined(AQUILA_PLATFORM_MACOS)
	std::string directory = from_env("HOME");
	if (!directory.empty()) {
		directory += "/Library/Caches";
	}
#else
	std::string directory = from_env("XDG_CACHE_HOME");
	if (directory.empty() || !path_is_absolute(directory)) {
		directory = from_env("HOME");
		if (!directory.empty()) {
			directory += "/.cache";
		}
	}
#endif
	return directory.empty() ? dir_get_current() : directory;
}

bool dir_set_current(const std::string &path) {
#ifdef AQUILA_PLATFORM_WINDOWS
	return SetCurrentDirectoryA(path.c_str()) != 0; // was missing .c_str() and != 0
#elif defined(AQUILA_PLATFORM_LINUX) || defined(AQUILA_PLATFORM_MACOS)
	return (chdir(path.c_str()) == 0);
#endif
}

bool dir_create(const std::string &path) {
#ifdef AQUILA_PLATFORM_WINDOWS
	return _mkdir(path.c_str()) == 0;
#elif defined(AQUILA_PLATFORM_LINUX) || defined(AQUILA_PLATFORM_MACOS)
	return (mkdir(path.c_str(), 0755) == 0);
#endif
}

bool dir_remove(const std::string &path) {
#ifdef AQUILA_PLATFORM_WINDOWS
	return RemoveDirectoryA(path.c_str()) != 0;
#elif defined(AQUILA_PLATFORM_LINUX) || defined(AQUILA_PLATFORM_MACOS)
	return (rmdir(path.c_str()) == 0);
#endif
}

bool file_remove(const std::string &path) {
#ifdef AQUILA_PLATFORM_WINDOWS
	return DeleteFileA(path.c_str()) != 0;
#elif defined(AQUILA_PLATFORM_LINUX) || defined(AQUILA_PLATFORM_MACOS)
	return (::remove(path.c_str()) == 0); // :: to avoid clashing with std::remove
#endif
}

bool file_move(const std::string &from, const std::string &to) {
#ifdef AQUILA_PLATFORM_WINDOWS
	return MoveFileExA(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#elif defined(AQUILA_PLATFORM_LINUX) || defined(AQUILA_PLATFORM_MACOS)
	return (std::rename(from.c_str(), to.c_str()) == 0); // :: to avoid any std::rename ambiguity
#endif
}

std::vector<std::string> dir_list(const std::string &path, bool recursive) {
	std::vector<std::string> entries;
#ifdef AQUILA_PLATFORM_WINDOWS
	WIN32_FIND_DATAA find_data;
	HANDLE h_find = FindFirstFileA((path + "\\*").c_str(), &find_data);
	if (h_find != INVALID_HANDLE_VALUE) {
		do {
			if (strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0) {
				entries.push_back(find_data.cFileName);
				if (recursive && ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u)) {
					auto sub = dir_list(path + "\\" + find_data.cFileName, true);
					entries.insert(entries.end(), sub.begin(), sub.end());
				}
			}
		} while (FindNextFileA(h_find, &find_data) != 0);
		FindClose(h_find);
	}
#elif defined(AQUILA_PLATFORM_LINUX) || defined(AQUILA_PLATFORM_MACOS)
	DIR *dir = opendir(path.c_str());
	if (dir) {
		struct dirent *entry;
		while ((entry = readdir(dir)) != nullptr) {
			if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
				entries.push_back(entry->d_name);
				if (recursive && entry->d_type == DT_DIR) {
					auto sub = dir_list(path + "/" + entry->d_name, true);
					entries.insert(entries.end(), sub.begin(), sub.end());
				}
			}
		}
		closedir(dir);
	}
#endif
	return entries;
}

std::string path_to_absolute(const std::string &path) {
	if (path_is_absolute(path)) {
		return path;
	}
	return path_join(dir_get_current(), path);
}

std::string path_extension(const std::string &path) {
	const size_t pos = path.find_last_of('.');
	// pos == 0 catches ".hidden", pos == npos catches no dot,
	// check no separator after dot to avoid "dir.name/file" false positives
	if (pos == std::string::npos || pos == 0) {
		return {};
	}
	const size_t sep = path.find_last_of("/\\");
	if (sep != std::string::npos && sep > pos) {
		return {};
	}
	return path.substr(pos); // includes the dot, e.g. ".txt"
}

} // namespace Aquila::Platform::Filesystem
