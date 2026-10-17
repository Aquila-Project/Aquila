#ifndef AQUILA_UI_TEXT_TEXT_SELECTION_H
#define AQUILA_UI_TEXT_TEXT_SELECTION_H

#include "Aquila/Foundation/PrimitiveTypes.h"

#include <compare>

namespace Aquila::UI::Text {

struct TextPosition {
	Int32 line = 0;
	Int32 column = 0;

	auto operator<=>(const TextPosition &) const = default;
};

struct TextSelection {
	TextPosition anchor;
	TextPosition focus;

	[[nodiscard]] bool is_empty() const { return anchor == focus; }
	[[nodiscard]] TextPosition begin() const { return std::min(anchor, focus); }
	[[nodiscard]] TextPosition end() const { return std::max(anchor, focus); }
};

/// The selected text, lines joined with '\n'.
[[nodiscard]] std::string extract_text(std::span<const std::string> lines, const TextSelection &selection);

/// The column closest to `x`. `measure` returns the width of a piece of the line.
[[nodiscard]] Int32 column_at(std::string_view line, F32 x, const std::function<F32(std::string_view)> &measure);

[[nodiscard]] TextSelection select_all(std::span<const std::string> lines);

} // namespace Aquila::UI::Text

#endif
