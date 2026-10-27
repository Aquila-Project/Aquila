#ifndef AQUILA_UI_MARKUP_MARKUP_PARSER_H
#define AQUILA_UI_MARKUP_MARKUP_PARSER_H

#include "Aquila/UI/Markup/MarkupDocument.h"

namespace Aquila::UI::Markup {

[[nodiscard]] MarkupDocument parse_markup(std::string_view text, std::string source_name);

}

#endif
