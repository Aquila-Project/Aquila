#include "Aquila/UI/DevTools/WidgetGalleryWindow.h"

#include "Aquila/UI/Core/UIResources.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Checkbox.h"
#include "Aquila/UI/Widgets/DragFloat.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/Tooltip.h"

namespace Aquila::UI::DevTools {

using namespace Aquila::UI::Core;

WidgetGalleryWindow::WidgetGalleryWindow() : WindowBehaviour(std::string(UI_DIR) + "Windows/widget_gallery.aqlayout") {}

void WidgetGalleryWindow::define_behaviour() {
	if (auto *grid = get<PropertyGrid>("gallery-grid")) {
		grid->add_row<DragFloat>("Scale");
		grid->add_row<Checkbox>("Visible");
	}

	auto *tooltip = get<Tooltip>("gallery-tooltip");
	auto *tooltip_button = get<Button>("gallery-tooltip-button");
	if (tooltip != nullptr && tooltip_button != nullptr) {
		tooltip_button->on_click.connect([tooltip, tooltip_button] {
			const Rect rect = tooltip_button->get_absolute_rect();
			tooltip->show_at({ rect.position.x, rect.position.y + rect.size.y + 4.F }, "Hello from a Tooltip!");
		});
	}
}

}
