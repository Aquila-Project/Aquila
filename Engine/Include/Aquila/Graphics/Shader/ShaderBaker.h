#ifndef AQUILA_GRAPHICS_SHADER_BAKER_H
#define AQUILA_GRAPHICS_SHADER_BAKER_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Graphics::Shader {

struct ShaderBakeReport {
	Usize programs = 0;
	Usize modules = 0;
	std::vector<std::string> errors;

	[[nodiscard]] bool ok() const { return errors.empty(); }
};
ShaderBakeReport bake_engine_shaders(const std::string &source_dir, const std::string &out_dir,
									 std::span<const std::string> skip = {});

ShaderBakeReport bake_project_shaders(const std::string &project_dir, const std::string &pack_path);

}

#endif
