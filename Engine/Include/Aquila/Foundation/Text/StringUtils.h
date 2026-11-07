#ifndef AQUILA_FOUNDATION_STRING_UTILS_H
#define AQUILA_FOUNDATION_STRING_UTILS_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Foundation {

[[nodiscard]] inline char to_lower(char c) {
	return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

/// ASCII only, UTF-8 bytes are left alone.
inline std::string to_lower(std::string_view text) {
	std::string result(text);
	std::ranges::transform(result, result.begin(), [](char c) { return to_lower(c); });
	return result;
}

[[nodiscard]] inline bool equals_ignore_case(std::string_view a, std::string_view b) {
	return std::ranges::equal(a, b, [](char x, char y) { return to_lower(x) == to_lower(y); });
}

/// An empty `word` is always found.
[[nodiscard]] inline bool contains_ignore_case(std::string_view text, std::string_view word) {
	return word.empty() ||
		!std::ranges::search(text, word, [](char x, char y) { return to_lower(x) == to_lower(y); }).empty();
}

[[nodiscard]] inline bool ends_with_ignore_case(std::string_view text, std::string_view suffix) {
	return text.size() >= suffix.size() && equals_ignore_case(text.substr(text.size() - suffix.size()), suffix);
}

/// Points into `text`.
[[nodiscard]] inline std::string_view trim(std::string_view text) {
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
		text.remove_prefix(1);
	}
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
		text.remove_suffix(1);
	}
	return text;
}

/// Pieces are trimmed, empty ones dropped.
[[nodiscard]] inline std::vector<std::string> split(std::string_view text, char delimiter) {
	std::vector<std::string> pieces;
	while (!text.empty()) {
		const Usize end = text.find(delimiter);
		if (const std::string_view piece = trim(text.substr(0, end)); !piece.empty()) {
			pieces.emplace_back(piece);
		}
		if (end == std::string_view::npos) {
			break;
		}
		text.remove_prefix(end + 1);
	}
	return pieces;
}

/// Splits a command line on spaces. Double or single quotes keep spaces in, and are dropped. An unclosed quote runs
/// to the end. No escapes.
[[nodiscard]] inline std::vector<std::string> split_arguments(std::string_view text) {
	std::vector<std::string> arguments;
	std::string current;
	bool started = false;
	char quote = 0;
	for (const char c : text) {
		if (quote != 0) {
			if (c == quote) {
				quote = 0;
			} else {
				current += c;
			}
		} else if (c == '"' || c == '\'') {
			quote = c;
			started = true;
		} else if (std::isspace(static_cast<unsigned char>(c)) != 0) {
			if (started) {
				arguments.push_back(std::move(current));
				current.clear();
				started = false;
			}
		} else {
			current += c;
			started = true;
		}
	}
	if (started) {
		arguments.push_back(std::move(current));
	}
	return arguments;
}

/// Replaces every `from` in `text`, left to right. What it puts in isn't searched again.
inline void replace_all(std::string &text, std::string_view from, std::string_view to) {
	if (from.empty()) {
		return;
	}
	for (Usize at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
		text.replace(at, from.size(), to);
	}
}

/// Letters, digits, '-' and '_' stay, spaces become '_', the rest is dropped. Can come back empty.
[[nodiscard]] inline std::string to_file_name(std::string_view name) {
	std::string out;
	out.reserve(name.size());
	for (const char c : name) {
		if (std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '_') {
			out += c;
		} else if (c == ' ') {
			out += '_';
		}
	}
	return out;
}

/// 1234567 becomes "1,234,567".
inline std::string with_separators(Uint64 value) {
	std::string digits = std::to_string(value);
	for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3) {
		digits.insert(static_cast<Usize>(i), ",");
	}
	return digits;
}

}

#endif
