#include "UI/Components/DockTabActions.h"

#include "Aquila/UI/Widgets/Button.h"

namespace Editor {

using Aquila::UI::Core::Button;

void DockTabActions::define_behaviour() {
	const auto wire = [this](std::string_view id, Signal<void(Vec2)> &signal) {
		Button *button = get<Button>(id);
		if (button == nullptr) {
			return;
		}
		listen(button->on_click, [button, &signal] {
			const Rect rect = button->get_absolute_rect();
			signal(Vec2(rect.position.x, rect.position.y + rect.size.y));
		});
	};
	wire("change", on_change_editor);
	wire("add", on_add_tab);
}

}
