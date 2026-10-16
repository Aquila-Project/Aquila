#pragma once

namespace Aquila::UI::Core {
struct DragState {
	std::any payload;
	std::string label;
	bool is_dragging;
};
} // namespace Aquila::UI::Core
