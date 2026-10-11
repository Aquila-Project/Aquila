#include "Core/ContentCreators.h"

#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"

namespace Editor {

using Aquila::Platform::Filesystem::VirtualFileSystem;

ContentCreator folder_creator() {
	return {
		.label = "Folder",
		.icon = "folder",
		.name = "NewFolder",
		.create = [](const std::string &path) { return VirtualFileSystem::get()->create_dir(path); },
	};
}

ContentCreator template_creator(std::string label, std::string icon, std::string name, std::string template_path) {
	return {
		.label = std::move(label),
		.icon = std::move(icon),
		.name = std::move(name),
		.create =
			[template_path = std::move(template_path)](const std::string &path) {
				auto *vfs = VirtualFileSystem::get();
				return vfs->write_text_file(path, vfs->read_text_file(template_path));
			},
	};
}

}
