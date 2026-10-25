#ifndef AQUILA_UI_MARKUP_MARKUP_PATH_H
#define AQUILA_UI_MARKUP_MARKUP_PATH_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::UI::Markup {

[[nodiscard]] std::string resolve_markup_path(std::string_view base_dir, std::string_view src,
											  std::string_view extension);

[[nodiscard]] std::string markup_directory(std::string_view path);

}

#endif
