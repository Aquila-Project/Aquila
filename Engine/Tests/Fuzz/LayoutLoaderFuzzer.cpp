#include "Aquila/Foundation/Log.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/UI/Core/LayoutLoader.h"

using namespace Aquila;

extern "C" int LLVMFuzzerInitialize(int *, char ***) {
	Foundation::Logger::set_log_level(Foundation::LogLevel::Critical);
	Platform::Filesystem::VirtualFileSystem::init();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const Uint8 *data, Usize size) {
	UI::Core::LayoutLoader loader;
	Unique<UI::Core::View> view = loader.load_string(std::string_view(reinterpret_cast<const char *>(data), size));
	return 0;
}
