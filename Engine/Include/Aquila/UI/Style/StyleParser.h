#pragma once

#include "Aquila/UI/Style/StyleProperties.h"
#include "Aquila/UI/Style/StyleParserHelper.h"

namespace Aquila::UI {

class StyleSheet;

/// Reads .aqstyle, a small CSS subset. Problems are logged with a location and the rule is skipped.
class StyleParser {
  public:
	static bool load_file(const std::string &path, StyleSheet &sheet);
	/// VFS paths, loaded in order into one sheet.
	static bool load_files(std::span<const std::string> paths, StyleSheet &sheet);

	/// Same as load_file() for text in memory. Errors say `<string>` for the file.
	static bool LoadString(std::string_view css, StyleSheet &sheet, bool clear_first = true);

	/// One declaration, like apply_property(props, "width", "120px").
	static void apply_property(StyleProperties &props, std::string_view property, std::string_view value);
};

} // namespace Aquila::UI
