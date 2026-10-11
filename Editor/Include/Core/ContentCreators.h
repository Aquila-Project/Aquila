#pragma once

#include "Aquila/Foundation/PrimitiveTypes.h"

#include <string>

namespace Editor {

struct ContentCreator {
	std::string label;
	std::string icon = "file";
	std::string name;
	Delegate<bool(const std::string &path)> create;
};

[[nodiscard]] ContentCreator folder_creator();
[[nodiscard]] ContentCreator template_creator(std::string label, std::string icon, std::string name,
											  std::string template_path);

}
