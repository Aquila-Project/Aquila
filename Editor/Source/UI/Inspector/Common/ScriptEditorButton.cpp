#include "UI/Inspector/Common/ScriptEditorButton.h"

#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Core/EditorContext.h"

namespace Editor {

Aquila::UI::Core::Button *add_script_editor_button(Aquila::UI::Core::PropertyGrid &grid, EditorContext &context) {
	auto *button = grid.add_row<Aquila::UI::Core::Button>("", std::string("Open in Script Editor"));
	if (button != nullptr) {
		button->set_icon(context.icon("file-code"));
		button->set_tooltip("Settings > Script Editor picks the program");
	}
	return button;
}

}
