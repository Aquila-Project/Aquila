#ifndef AQUILA_UI_CORE_DOCK_LAYOUT_BUILDER_H
#define AQUILA_UI_CORE_DOCK_LAYOUT_BUILDER_H

#include "Aquila/UI/Core/DockLayoutSerializer.h"

#include <initializer_list>

namespace Aquila::UI::Core::DockLayout {

/// A child of a row or column and its share of the space.
///
/// @code
/// using namespace DockLayout;
/// layout(row({
///     { 0.2F, tabs({ "hierarchy" }) },
///     { 0.6F, column({ { 0.7F, tabs({ "viewport" }) }, { 0.3F, tabs({ "content", "console" }) } }) },
///     { 0.2F, tabs({ "inspector" }) },
/// }));
/// @endcode
using Part = std::pair<F32, DockNodeDesc>;

[[nodiscard]] DockNodeDesc tabs(std::initializer_list<std::string> panel_ids, Int32 active = 0);
[[nodiscard]] DockNodeDesc row(std::initializer_list<Part> parts);
[[nodiscard]] DockNodeDesc column(std::initializer_list<Part> parts);
[[nodiscard]] DockLayoutDesc layout(DockNodeDesc root);

}

#endif
