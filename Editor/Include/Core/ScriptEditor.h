#pragma once

namespace Editor {

/// Opens a file (a VFS path) in the program set in Settings > Script Editor, or with the system's default app when
/// none is set. Logs why when it can't: no file on disk behind the path, or the program didn't start.
bool open_in_script_editor(const std::string &path);

}
