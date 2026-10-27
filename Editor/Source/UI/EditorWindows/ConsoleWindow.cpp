#include "UI/EditorWindows/ConsoleWindow.h"

#include "UI/Panels/ConsolePanel.h"

namespace Editor {

ConsoleWindow::ConsoleWindow(EditorContext &context) : EditorWindow(context, context.window_layout("console")) {}

ConsoleWindow::~ConsoleWindow() = default;

void ConsoleWindow::define_behaviour() {
	m_panel = std::make_unique<ConsolePanel>();
	m_panel->build(&root(), overlay_root());
}

void ConsoleWindow::update(F32) {
	m_panel->flush_pending();
}

void ConsoleWindow::clear() {
	m_panel->clear_all();
}

}
