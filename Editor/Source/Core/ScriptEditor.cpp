#include "Core/ScriptEditor.h"

#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Platform/Process.h"
#include "Core/EditorConfig.h"

namespace Editor {

bool open_in_script_editor(const std::string &path) {
	auto *vfs = Aquila::Platform::Filesystem::VirtualFileSystem::get();
	const Option<std::string> file = vfs != nullptr ? vfs->to_native_path(path) : std::nullopt;
	if (!file) {
		AQUILA_LOG_ERROR("Script editor: {} isn't a file on disk", path);
		return false;
	}

	const Config::ScriptEditorPreferences &preferences = Config::get_preferences().script_editor;
	const std::string program(Aquila::Foundation::trim(preferences.program));
	if (program.empty()) {
		if (!Aquila::Platform::open_url(*file)) {
			AQUILA_LOG_ERROR("Script editor: the system couldn't open {}", *file);
			return false;
		}
		return true;
	}

	const std::string project =
		vfs->to_native_path(std::string(Aquila::SharedConstants::PROJECT_MOUNT)).value_or(std::string{});
	std::vector<std::string> arguments = Aquila::Foundation::split_arguments(preferences.arguments);
	bool has_file = false;
	for (std::string &argument : arguments) {
		has_file = has_file || argument.find("{file}") != std::string::npos;
		Aquila::Foundation::replace_all(argument, "{project}", project);
		Aquila::Foundation::replace_all(argument, "{file}", *file);
	}
	if (!has_file) {
		arguments.push_back(*file);
	}
	if (!Aquila::Platform::launch_detached(program, arguments)) {
		AQUILA_LOG_ERROR("Script editor: couldn't start '{}', check Settings > Script Editor", program);
		return false;
	}
	return true;
}

}
