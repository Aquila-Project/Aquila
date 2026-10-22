#include "Aquila/Foundation/Log.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Style/StyleSheet.h"

using namespace Aquila;

extern "C" int LLVMFuzzerInitialize(int *, char ***) {
	Foundation::Logger::set_log_level(Foundation::LogLevel::Critical);
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const Uint8 *data, Usize size) {
	UI::StyleSheet sheet;
	UI::StyleParser::LoadString(std::string_view(reinterpret_cast<const char *>(data), size), sheet);
	return 0;
}
