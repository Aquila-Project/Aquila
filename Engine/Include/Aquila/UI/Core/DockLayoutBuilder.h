#ifndef AQUILA_UI_CORE_DOCK_LAYOUT_BUILDER_H
#define AQUILA_UI_CORE_DOCK_LAYOUT_BUILDER_H

#include "Aquila/UI/Core/DockLayoutSerializer.h"

#include <initializer_list>

namespace Aquila::UI::Core::DockLayout {

using Part = std::pair<F32, DockNodeDesc>;

[[nodiscard]] DockNodeDesc tabs(std::initializer_list<std::string> panel_ids, Int32 active = 0);
[[nodiscard]] DockNodeDesc row(std::initializer_list<Part> parts);
[[nodiscard]] DockNodeDesc column(std::initializer_list<Part> parts);
[[nodiscard]] DockLayoutDesc layout(DockNodeDesc root);

}

#endif
