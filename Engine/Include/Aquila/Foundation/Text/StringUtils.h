#ifndef AQUILA_FOUNDATION_STRING_UTILS_H
#define AQUILA_FOUNDATION_STRING_UTILS_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Foundation {

inline std::string to_lower(std::string_view text) {
	std::string result(text);
	std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return result;
}

inline std::string with_separators(Uint64 value) {
	std::string digits = std::to_string(value);
	for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3) {
		digits.insert(static_cast<Usize>(i), ",");
	}
	return digits;
}

}

#endif
