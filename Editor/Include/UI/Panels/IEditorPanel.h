#pragma once

#include "Aquila/Foundation/Signal.h"

namespace Aquila::UI::Core {
class View;
} // namespace Aquila::UI::Core

namespace Editor {

/// The older panel interface. The *Window classes wrap these panels as EditorWindows.
class IEditorPanel {
  public:
	virtual ~IEditorPanel() = default;
	virtual void build(Aquila::UI::Core::View *panel, Aquila::UI::Core::View *overlay_root) = 0;
};

} // namespace Editor
