#pragma once

#include "Aquila/UI/Widgets/Inspectable.h"

namespace Editor {

class EditorContext;

/// The icon of `path`'s asset type, "file" when no type matches.
[[nodiscard]] std::string asset_icon(const std::string &path, EditorContext &context);

/// A file's inspectable: its asset type's icon, the file name without the extension as the title, the path as the
/// subtitle and one tab, named after the type, holding `sections`.
[[nodiscard]] Aquila::UI::Core::Inspectable
make_asset_inspectable(const std::string &path, EditorContext &context,
					   std::vector<Aquila::UI::Core::InspectorSection> sections);

}
