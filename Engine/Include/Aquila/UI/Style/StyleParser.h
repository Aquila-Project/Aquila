#pragma once

#include "Aquila/UI/Style/StyleProperties.h"
#include "Aquila/UI/Style/StyleParserHelper.h"

namespace Aquila::UI {

class StyleSheet;

class StyleParser {
  public:
	static bool load_file(const std::string &path, StyleSheet &sheet);
	static bool load_files(std::span<const std::string> paths, StyleSheet &sheet);

	static bool LoadString(std::string_view css, StyleSheet &sheet, bool clear_first = true);

	static void apply_property(StyleProperties &props, std::string_view property, std::string_view value);
};

} // namespace Aquila::UI
