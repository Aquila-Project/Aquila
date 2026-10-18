#ifndef AQUILA_PLATFORM_PROCESS_H
#define AQUILA_PLATFORM_PROCESS_H

namespace Aquila::Platform {

/// Starts a process and returns right away. `executable` is looked up in PATH.
/// @note On Windows the child starts in the executable's folder, on Linux in ours.
bool launch_detached(const std::string &executable, const std::vector<std::string> &arguments = {});
/// Opens it with the system default app (xdg-open, explorer).
bool open_url(const std::string &url);
/// Opens the system file manager with `native_path` selected. On Linux it asks the file manager over D-Bus
/// (org.freedesktop.FileManager1) and opens the folder with xdg-open when nothing answers.
/// Returns right away, the file manager starts on its own.
bool show_in_file_manager(const std::string &native_path);
/// "Show in Explorer", "Reveal in Finder" or "Show in File Manager", for menus.
[[nodiscard]] const char *file_manager_action_name();

} // namespace Aquila::Platform

#endif
