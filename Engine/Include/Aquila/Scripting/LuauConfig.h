#ifndef AQUILA_SCRIPTING_LUAU_CONFIG_H
#define AQUILA_SCRIPTING_LUAU_CONFIG_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Scripting {

/// What luau-lsp needs to follow the engine API: "aquila" to `api_dir`, and one alias per native module type file
/// that ScriptSystem::write_types left in `types_dir` (globals.d.luau isn't a module). Native paths with '/'.
[[nodiscard]] std::map<std::string, std::string> engine_luau_aliases(const std::string &api_dir,
																	 const std::string &types_dir);

/// Merges `aliases` into `directory`/.luaurc: its other keys and aliases stay, languageMode becomes strict when it
/// has none. Only writes when something changes. A file with comments or broken JSON is left alone and logged,
/// rewriting it would lose them. False when the file isn't up to date afterwards.
bool update_luaurc(const std::string &directory, const std::map<std::string, std::string> &aliases);

/// Same rules for `directory`/.vscode/settings.json: points luau-lsp's "@aquila" definitions at `definitions_file`.
bool update_vscode_settings(const std::string &directory, const std::string &definitions_file);

}

#endif
