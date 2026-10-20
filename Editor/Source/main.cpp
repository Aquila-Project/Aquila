#include "Core/EditorModule.h"

#include "Aquila/Application/ApplicationNew.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Scripting/ScriptModule.h"

int main(int argc, char **argv) {
	std::string project;
	for (int i = 1; i + 1 < argc; ++i) {
		if (std::string_view(argv[i]) == "--project") {
			project = argv[i + 1];
		}
	}

	ApplicationSpec spec;
	spec.name = std::string("Aquila Studio | ") + AQUILA_VERSION_STRING;
	spec.width = 1920;
	spec.height = 1080;
	spec.start_hidden = true;
	spec.icon = Aquila::Foundation::EnginePaths::resources_dir() + "Engine/aquila-logo.png";

	Aquila::Application::Application app(spec);
	app.add_module<Aquila::Scripting::ScriptModule>();
	app.add_module<Editor::EditorModule>(project);
	app.run();
	return EXIT_SUCCESS;
}
