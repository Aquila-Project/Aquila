#ifndef AQUILA_UI_CORE_UI_RESOURCES_H
#define AQUILA_UI_CORE_UI_RESOURCES_H

namespace Aquila::UI::Core {

inline constexpr std::string_view UI_DIR = "/resources/Engine/UI/";
inline constexpr std::string_view UI_COMPONENTS_DIR = "/resources/Engine/UI/Components/";

[[nodiscard]] std::string engine_component_file(std::string_view name);

}

#endif
