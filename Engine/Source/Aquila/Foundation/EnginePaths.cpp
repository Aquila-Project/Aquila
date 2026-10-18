#include "Aquila/Foundation/EnginePaths.h"

namespace Aquila::Foundation::EnginePaths {

namespace {

std::string with_slash(std::string_view directory) {
	std::string out(directory);
	if (!out.empty() && out.back() != '/' && out.back() != '\\') {
		out += '/';
	}
	return out;
}

std::string &shaders() {
	static std::string directory = AQUILA_SHADERS_DIR;
	return directory;
}

std::string &resources() {
	static std::string directory = AQUILA_RESOURCES_DIR;
	return directory;
}

} // namespace

void set_shaders_dir(std::string_view directory) {
	shaders() = with_slash(directory);
}

void set_resources_dir(std::string_view directory) {
	resources() = with_slash(directory);
}

const std::string &shaders_dir() {
	return shaders();
}

const std::string &resources_dir() {
	return resources();
}

} // namespace Aquila::Foundation::EnginePaths
