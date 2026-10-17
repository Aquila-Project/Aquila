#pragma once

namespace Aquila::UI::Core {
/// `payload` is whatever the drag source put in, check it with std::any_cast before using it.
struct DragState {
	std::any payload;
	std::string label;
	bool is_dragging;
};
} // namespace Aquila::UI::Core
