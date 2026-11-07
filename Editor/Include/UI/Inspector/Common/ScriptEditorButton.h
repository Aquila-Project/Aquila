#pragma once

namespace Aquila::UI::Core {
class Button;
class PropertyGrid;
}

namespace Editor {

class EditorContext;

/// The "Open in Script Editor" row. Connect on_click yourself. Null outside a canvas.
Aquila::UI::Core::Button *add_script_editor_button(Aquila::UI::Core::PropertyGrid &grid, EditorContext &context);

}
