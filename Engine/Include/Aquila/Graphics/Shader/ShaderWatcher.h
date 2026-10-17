#ifndef AQUILA_SHADER_WATCHER_H
#define AQUILA_SHADER_WATCHER_H

#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"

namespace Aquila::Graphics::Shader {

struct WatchedShader {
	std::string slang_path; // original path as supplied (VFS or native)
	std::string program_name;
	Uint64 last_modified = 0;
	bool is_native = false; // true = bypass VFS, use std::filesystem directly
};

/// @note Only the file itself is watched. Editing a module it imports doesn't trigger a reload.
class ShaderWatcher {
  public:
	ShaderWatcher() = default;

	void enable(bool enable) { m_enabled = enable; }
	bool is_enabled() const { return m_enabled; }

	// Accepts either:
	//   - A VFS virtual path  (e.g. "assets://Shaders/GBuffer.slang")
	//   - A native absolute path (e.g. "C:/Programming/Aquila/Engine/Shaders/GBuffer.slang")
	void watch_slang_file(const std::string &slang_path, const std::string &program_name) {
		bool native = is_native_path(slang_path);

		if (native) {
			if (!std::filesystem::exists(slang_path)) {
				AQUILA_LOG_WARNING("ShaderWatcher: cannot watch non-existent file '{}'", slang_path);
				return;
			}
			Uint64 last_write = native_last_write_time(slang_path);
			m_watched_files[slang_path] = { .slang_path = slang_path,
										  .program_name = program_name,
										  .last_modified = last_write,
										  .is_native = true };
		} else {
			auto *vfs = Platform::Filesystem::VirtualFileSystem::get();
			if (!vfs->exists(slang_path)) {
				AQUILA_LOG_WARNING("ShaderWatcher: cannot watch non-existent VFS file '{}'", slang_path);
				return;
			}
			m_watched_files[slang_path] = { .slang_path = slang_path,
										  .program_name = program_name,
										  .last_modified = vfs->get_last_write_time(slang_path),
										  .is_native = false };
		}

		AQUILA_LOG_INFO("ShaderWatcher: watching '{}' ({}) -> program '{}'", slang_path, native ? "native" : "VFS",
						program_name);
	}

	void unwatch(const std::string &slang_path) { m_watched_files.erase(slang_path); }
	void clear() { m_watched_files.clear(); }

	// Returns the set of program names that need to be reloaded.
	std::unordered_set<std::string> check_for_changes() {
		if (!m_enabled) {
			return {};
		}

		std::unordered_set<std::string> changed;
		auto *vfs = Platform::Filesystem::VirtualFileSystem::get();

		for (auto &[path, watch] : m_watched_files) {
			Uint64 current = 0;

			if (watch.is_native) {
				if (!std::filesystem::exists(path)) {
					continue;
				}
				current = native_last_write_time(path);
			} else {
				if (!vfs->exists(path)) {
					continue;
				}
				current = vfs->get_last_write_time(path);
			}

			if (current > watch.last_modified) {
				AQUILA_LOG_INFO("ShaderWatcher: '{}' modified, queuing reload of program '{}'", path,
								watch.program_name);
				watch.last_modified = current;
				changed.insert(watch.program_name);
			}
		}

		return changed;
	}

  private:
	// A path is treated as native if it is an absolute filesystem path
	// (starts with a drive letter on Windows, or '/' on Unix) and does NOT
	// contain "://" which is the VFS scheme separator.
	static bool is_native_path(const std::string &path) {
		if (path.find("://") != std::string::npos) {
			return false; // VFS virtual path
		}

		// std::filesystem considers "C:\..." and "/home/..." as absolute
		return std::filesystem::path(path).is_absolute();
	}

	static Uint64 native_last_write_time(const std::string &path) {
		std::error_code ec;
		auto ftime = std::filesystem::last_write_time(path, ec);
		if (ec) {
			return 0;
		}
		// Convert to a plain integer comparable across calls
		return static_cast<Uint64>(ftime.time_since_epoch().count());
	}

	std::unordered_map<std::string, WatchedShader> m_watched_files;
	bool m_enabled = false;
};

} // namespace Aquila::Graphics::Shader
#endif // AQUILA_SHADER_WATCHER_H
