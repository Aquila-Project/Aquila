#pragma once

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/UI/Style/StyleLength.h"
#include "Aquila/UI/Style/StyleProperties.h"
#include "Aquila/UI/Style/StyleSheet.h"

namespace Aquila::UI::ParserHelper {

std::string_view trim_sv(std::string_view s);
std::string trim(std::string_view s);
using Foundation::to_lower;

std::vector<std::string> split(std::string_view s, char delim);

std::vector<std::string> split_ws(std::string_view s);

std::string strip_comments(std::string_view src);

float parse_float(std::string_view s);

float parse_duration_ms(std::string_view s);

std::optional<TransitionEasing> parse_easing(std::string_view s);
/// "auto", "grow" (or "1fr"), px, %, vw, vh. A bare number is pixels.
Option<StyleLength> parse_length(std::string_view raw);
/// "transparent", #rrggbb, #rrggbbaa, rgb() and rgba().
/// @note rgb() is 0-255, but when no channel is above 1 it's read as 0-1, so rgb(1, 1, 1) is white.
Option<Vec4> parse_color(std::string_view raw);

Option<StyleEdges> parse_edges(std::string_view s);
Option<Vec4> parse_radius(std::string_view s);
Option<BoxShadow> parse_one_shadow(std::string_view raw);

void apply_declaration(StyleProperties &props, std::string_view prop_raw, std::string_view value_raw);
void parse_block(std::string_view selector_list, std::string_view body, StyleSheet &sheet);

} // namespace Aquila::UI::ParserHelper
