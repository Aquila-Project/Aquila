#pragma once

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Editor {

/// An entry of the content browser's New menu. `name` is the default file name,
/// `create` gets the full VFS path of the new item.
struct ContentCreator {
	std::string label;
	std::string icon = "file";
	std::string name;
	Delegate<bool(const std::string &path)> create;
};

[[nodiscard]] ContentCreator folder_creator();
/// Creates the file as a copy of `template_path`.
[[nodiscard]] ContentCreator template_creator(std::string label, std::string icon, std::string name,
											  std::string template_path);

}
