#include "Aquila/Platform/Process.h"

#ifndef AQUILA_PLATFORM_WINDOWS
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;
#endif

namespace Aquila::Platform {

#ifdef AQUILA_PLATFORM_WINDOWS

bool launch_detached(const std::string &executable, const std::vector<std::string> &arguments) {
	std::string command = "\"" + executable + "\"";
	for (const std::string &argument : arguments) {
		command += " \"" + argument + "\"";
	}
	const std::string directory = std::filesystem::path(executable).parent_path().string();

	STARTUPINFOA startup{};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process{};
	if (CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr,
					   directory.empty() ? nullptr : directory.c_str(), &startup, &process) == 0) {
		return false;
	}
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return true;
}

#else

bool launch_detached(const std::string &executable, const std::vector<std::string> &arguments) {
	std::vector<char *> argv;
	argv.push_back(const_cast<char *>(executable.c_str()));
	for (const std::string &argument : arguments) {
		argv.push_back(const_cast<char *>(argument.c_str()));
	}
	argv.push_back(nullptr);

	pid_t pid = 0;
	if (posix_spawnp(&pid, executable.c_str(), nullptr, nullptr, argv.data(), environ) != 0) {
		return false;
	}
	std::thread([pid] { waitpid(pid, nullptr, 0); }).detach();
	return true;
}

#endif

bool open_url(const std::string &url) {
#ifdef AQUILA_PLATFORM_WINDOWS
	return launch_detached("explorer.exe", { url });
#else
	return launch_detached("xdg-open", { url });
#endif
}

#ifdef AQUILA_PLATFORM_WINDOWS

bool show_in_file_manager(const std::string &native_path) {
	const std::string path = std::filesystem::path(native_path).make_preferred().string();
	std::string command = "explorer.exe /select,\"" + path + "\"";
	STARTUPINFOA startup{};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process{};
	if (CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process) ==
		0) {
		return false;
	}
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return true;
}

const char *file_manager_action_name() {
	return "Show in Explorer";
}

#elif defined(AQUILA_PLATFORM_MACOS)

bool show_in_file_manager(const std::string &native_path) {
	return launch_detached("open", { "-R", native_path });
}

const char *file_manager_action_name() {
	return "Reveal in Finder";
}

#else

namespace {

std::string file_uri(const std::string &path) {
	constexpr std::string_view k_hex = "0123456789ABCDEF";
	std::string uri = "file://";
	for (const unsigned char c : path) {
		if (std::isalnum(c) != 0 || c == '/' || c == '-' || c == '_' || c == '.' || c == '~') {
			uri += static_cast<char>(c);
		} else {
			uri += '%';
			uri += k_hex[c >> 4];
			uri += k_hex[c & 15];
		}
	}
	return uri;
}

int run_and_wait(const std::string &executable, const std::vector<std::string> &arguments) {
	std::vector<char *> argv;
	argv.push_back(const_cast<char *>(executable.c_str()));
	for (const std::string &argument : arguments) {
		argv.push_back(const_cast<char *>(argument.c_str()));
	}
	argv.push_back(nullptr);

	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
	posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

	pid_t pid = 0;
	const int spawned = posix_spawnp(&pid, executable.c_str(), &actions, nullptr, argv.data(), environ);
	posix_spawn_file_actions_destroy(&actions);
	if (spawned != 0) {
		return -1;
	}
	int status = 0;
	if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status)) {
		return -1;
	}
	return WEXITSTATUS(status);
}

}

bool show_in_file_manager(const std::string &native_path) {
	std::error_code error;
	const std::filesystem::path path = std::filesystem::absolute(native_path, error);
	if (error || !std::filesystem::exists(path, error)) {
		return false;
	}
	std::thread([path] {
		const std::vector<std::string> show_items = {
			"call",
			"--session",
			"--dest",
			"org.freedesktop.FileManager1",
			"--object-path",
			"/org/freedesktop/FileManager1",
			"--method",
			"org.freedesktop.FileManager1.ShowItems",
			"['" + file_uri(path.string()) + "']",
			"",
		};
		if (run_and_wait("gdbus", show_items) == 0) {
			return;
		}
		std::error_code dir_error;
		const std::filesystem::path folder = std::filesystem::is_directory(path, dir_error) ? path : path.parent_path();
		run_and_wait("xdg-open", { folder.string() });
	}).detach();
	return true;
}

const char *file_manager_action_name() {
	return "Show in File Manager";
}

#endif

}
