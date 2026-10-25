#ifndef AQUILA_TESTS_UI_VFS_TEST_SUPPORT_H
#define AQUILA_TESTS_UI_VFS_TEST_SUPPORT_H

#include "Aquila/Foundation/Log.h"
#include "Aquila/Platform/Filesystem/NativeFileSystem.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"

inline Aquila::Platform::Filesystem::VirtualFileSystem &test_vfs() {
	using Aquila::Platform::Filesystem::VirtualFileSystem;
	static VirtualFileSystem *vfs = [] {
		VirtualFileSystem::init();
		VirtualFileSystem::get()->mount("/resources",
										std::make_shared<Aquila::Platform::Filesystem::NativeFileSystem>(
											std::string(AQUILA_SOURCE_DIR) + "/Resources"));
		return VirtualFileSystem::get();
	}();
	return *vfs;
}

struct MarkupFiles {
	static constexpr std::string_view K_MOUNT = "/markup-tests";

	MarkupFiles()
		: m_dir(
			  std::filesystem::temp_directory_path() /
			  ("aquila-markup-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
		std::filesystem::create_directories(m_dir);
		test_vfs().mount(std::string(K_MOUNT),
						 std::make_shared<Aquila::Platform::Filesystem::NativeFileSystem>(m_dir.string()));
	}

	~MarkupFiles() {
		test_vfs().unmount(std::string(K_MOUNT));
		std::error_code error;
		std::filesystem::remove_all(m_dir, error);
	}

	MarkupFiles(const MarkupFiles &) = delete;
	MarkupFiles &operator=(const MarkupFiles &) = delete;

	void write(const std::string &virtual_path, std::string_view text) const {
		const std::filesystem::path native = m_dir / virtual_path.substr(K_MOUNT.size() + 1);
		std::filesystem::create_directories(native.parent_path());
		std::ofstream file(native, std::ios::trunc);
		file << text;
	}

	std::filesystem::path m_dir;
};

struct LogCapture {
	std::ostringstream stream;
	Aquila::Foundation::LogLevel previous = Aquila::Foundation::Logger::get_log_level();

	LogCapture() {
		Aquila::Foundation::Logger::set_sink(&stream);
		Aquila::Foundation::Logger::set_log_level(Aquila::Foundation::LogLevel::Warning);
	}

	~LogCapture() {
		Aquila::Foundation::Logger::set_sink(nullptr);
		Aquila::Foundation::Logger::set_log_level(previous);
	}

	LogCapture(const LogCapture &) = delete;
	LogCapture &operator=(const LogCapture &) = delete;
};

#endif
