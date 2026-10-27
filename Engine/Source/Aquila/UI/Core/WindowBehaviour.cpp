#include "Aquila/UI/Core/WindowBehaviour.h"

namespace Aquila::UI::Core {

WindowBehaviour::WindowBehaviour(std::string layout_path) : m_layout_path(std::move(layout_path)) {}

void WindowBehaviour::close() {
	if (m_close) {
		m_close();
	}
}
}
