#pragma once

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Editor {

/// "move_speed" and "baseColorMap" to "Move Speed" and "Base Color Map", for rows named after code identifiers.
[[nodiscard]] inline std::string label_of(std::string_view name) {
	std::string label;
	bool word_start = true;
	for (Usize i = 0; i < name.size(); ++i) {
		const char c = name[i];
		if (c == '_' || c == ' ') {
			word_start = true;
			continue;
		}
		const bool camel_break = i > 0 && std::isupper(static_cast<unsigned char>(c)) != 0 &&
			std::islower(static_cast<unsigned char>(name[i - 1])) != 0;
		if ((word_start || camel_break) && !label.empty()) {
			label += ' ';
		}
		label += word_start || camel_break ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
		word_start = false;
	}
	return label;
}

}
