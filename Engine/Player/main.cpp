#include "Aquila/Application/ApplicationNew.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Platform/Filesystem/Filesystem.h"
#include "Aquila/Player/PlayerModule.h"
#include "Aquila/Scripting/ScriptModule.h"

int main() {
	using namespace Aquila;

	const Player::BuildLayout layout{ .root = Platform::Filesystem::path_executable_dir() };
	Option<Player::BuildManifest> manifest = Player::load_manifest(layout.manifest_file());
	if (!manifest) {
		AQUILA_LOG_ERROR("Player: no valid manifest at {}", layout.manifest_file());
		return EXIT_FAILURE;
	}

	ApplicationSpec spec;
	spec.name = manifest->window_title.empty() ? manifest->name : manifest->window_title;
	spec.width = manifest->window_width;
	spec.height = manifest->window_height;
	spec.maximized = manifest->maximized;
	spec.shader_hot_reload = false;
	spec.shaders_dir = layout.engine_shaders_dir();
	spec.resources_dir = layout.engine_resources_dir();
	spec.icon = std::filesystem::is_regular_file(layout.icon_file())
		? layout.icon_file()
		: (std::filesystem::path(layout.engine_resources_dir()) / "Engine" / "aquila-logo.png").string();

	Application::Application app(spec);
	app.add_module<Scripting::ScriptModule>();
	app.add_module<Player::PlayerModule>(layout, std::move(*manifest));
	app.run();
	return EXIT_SUCCESS;
}
