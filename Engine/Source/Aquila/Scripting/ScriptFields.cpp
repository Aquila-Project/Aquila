#include "Aquila/Scripting/ScriptFields.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scripting/ScriptBindings.h"

namespace Aquila::Scripting {

SceneManagement::Components::ScriptFieldValue field_value(const ScriptField &field,
														  const SceneManagement::Components::ScriptComponent &component) {
	if (const auto it = component.fields.find(field.name);
		it != component.fields.end() && it->second.index() == field.default_value.index()) {
		return it->second;
	}
	return field.default_value;
}

std::vector<ScriptField> read_script_fields(const std::string &path) {
	std::vector<ScriptField> declared;
	auto *vfs = Platform::Filesystem::VirtualFileSystem::get();
	if (path.empty() || vfs == nullptr || !vfs->exists(path)) {
		return declared;
	}

	SceneManagement::Scene scratch("Script fields");
	ScriptFieldContext context{ .declared = &declared };
	ScriptEnvironment environment{ .scene = &scratch, .loading = &context };
	aquiLua::Runtime runtime;
	runtime.set_error_handler([](std::string_view message) { AQUILA_LOG_ERROR("[Luau] {}", message); });
	register_engine_api(runtime, environment);
	runtime.unload(runtime.load_source(path, vfs->read_text_file(path)));
	return declared;
}

}
