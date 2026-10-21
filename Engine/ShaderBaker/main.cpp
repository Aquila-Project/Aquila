#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Graphics/Shader/ShaderBaker.h"

int main(int argc, char **argv) {
	if (argc < 3) {
		std::fprintf(stderr, "usage: AquilaShaderBaker <engine shaders folder> <output folder> [file to skip]...\n");
		return 2;
	}

	const std::vector<std::string> skip(argv + 3, argv + argc);
	Aquila::Foundation::EnginePaths::set_shaders_dir(argv[1]);
	const Aquila::Graphics::Shader::ShaderBakeReport report =
		Aquila::Graphics::Shader::bake_engine_shaders(Aquila::Foundation::EnginePaths::shaders_dir(), argv[2], skip);
	for (const std::string &error : report.errors) {
		std::fprintf(stderr, "%s\n", error.c_str());
	}
	if (!report.ok()) {
		std::fprintf(stderr, "[AQUILA] %zu engine shaders failed to bake\n", report.errors.size());
		return 1;
	}
	std::printf("[AQUILA] Baked %zu shader programs and %zu modules into %s\n", report.programs, report.modules,
				argv[2]);
	return 0;
}
