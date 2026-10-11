#include "Aquila/Platform/Filesystem/NativeFileSystem.h"
#include "Aquila/Platform/Filesystem/Filesystem.h"

#include <filesystem>

namespace Aquila::Platform::Filesystem {

NativeFileSystem::NativeFileSystem(const std::string &root_path) : m_root_path(Filesystem::path_normalize(root_path)) {
	if (!Filesystem::file_exists(m_root_path)) {
		Filesystem::dir_create(m_root_path);
	}
	if (!m_root_path.empty() && m_root_path.back() != '/' && m_root_path.back() != '\\') {
#ifdef AQUILA_PLATFORM_WINDOWS
		m_root_path += '\\';
#else
		m_root_path += '/';
#endif
	}
}

std::string NativeFileSystem::resolve_path(const std::string &path) const {
	const std::string clean = (!path.empty() && (path[0] == '/' || path[0] == '\\')) ? path.substr(1) : path;
	return Filesystem::path_normalize(Filesystem::path_join(m_root_path, clean));
}

std::string NativeFileSystem::to_fopen_mode(AccessMode access_mode, OpenMode open_mode) {
	const bool binary = has_flag(open_mode, OpenMode::Binary);
	const bool append = has_flag(open_mode, OpenMode::Append);
	const char *b = binary ? "b" : "";

	if (append) {
		return std::string("a") + b; // "a" or "ab"
	}

	switch (access_mode) {
	case AccessMode::Read:
		return std::string("r") + b; // "r"  or "rb"
	case AccessMode::Write:
		return std::string("w") + b; // "w"  or "wb"
	case AccessMode::ReadWrite:
		return std::string("r+") + b; // "r+" or "r+b"
	default:
		return std::string("r") + b;
	}
}

Unique<NativeFile> NativeFileSystem::file_open(const std::string &path, AccessMode access_mode, OpenMode open_mode) {
	FILE *file = nullptr;
#ifdef AQUILA_PLATFORM_WINDOWS
	fopen_s(&file, resolve_path(path).c_str(), to_fopen_mode(access_mode, open_mode).c_str());
#else
	file = fopen(resolve_path(path).c_str(), to_fopen_mode(access_mode, open_mode).c_str());
#endif
	return (file != nullptr) ? std::make_unique<NativeFile>(file) : nullptr;
}

bool NativeFileSystem::file_exists(const std::string &path) {
	return Filesystem::file_exists(resolve_path(path));
}

bool NativeFileSystem::file_remove(const std::string &path) {
	return Filesystem::file_remove(resolve_path(path));
}

bool NativeFileSystem::file_move(const std::string &old_path, const std::string &new_path) {
	const std::string full_old = resolve_path(old_path);
	const std::string full_new = resolve_path(new_path);
#ifdef AQUILA_PLATFORM_WINDOWS
	return MoveFileExA(full_old.c_str(), full_new.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
	return std::rename(full_old.c_str(), full_new.c_str()) == 0;
#endif
}

bool NativeFileSystem::file_copy(const std::string &src_path, const std::string &dst_path) {
	try {
		std::filesystem::copy(resolve_path(src_path), resolve_path(dst_path),
							  std::filesystem::copy_options::overwrite_existing);
		return true;
	} catch (const std::exception &) {
		return false;
	}
}

Int64 NativeFileSystem::file_get_size(const std::string &path) {
	const auto stat = Filesystem::file_stat(resolve_path(path));
	return stat.exists ? static_cast<Int64>(stat.size) : -1;
}

Uint64 NativeFileSystem::file_get_last_write_time(const std::string &path) {
	const auto stat = Filesystem::file_stat(resolve_path(path));
	return stat.exists ? stat.last_write_time : 0;
}

bool NativeFileSystem::dir_exists(const std::string &path) {
	const auto stat = Filesystem::file_stat(resolve_path(path));
	return stat.exists && stat.is_directory;
}

bool NativeFileSystem::dir_create(const std::string &path) {
	return Filesystem::dir_create(resolve_path(path));
}

bool NativeFileSystem::dir_remove(const std::string &path) {
	return Filesystem::dir_remove(resolve_path(path));
}

bool NativeFileSystem::dir_remove_all(const std::string &path) {
	std::string root = Filesystem::path_normalize(m_root_path);
	if (!root.ends_with('/')) {
		root += '/';
	}
	const std::string target = resolve_path(path);
	if (!target.starts_with(root) || target.size() <= root.size()) {
		return false;
	}
	std::error_code error;
	std::filesystem::remove_all(target, error);
	return !error;
}

std::vector<std::string> NativeFileSystem::dir_list(const std::string &path) {
	return Filesystem::dir_list(resolve_path(path), false);
}

} // namespace Aquila::Platform::Filesystem
