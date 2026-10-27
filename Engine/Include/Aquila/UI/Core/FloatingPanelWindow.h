#ifndef AQUILA_UI_CORE_FLOATING_PANEL_WINDOW_H
#define AQUILA_UI_CORE_FLOATING_PANEL_WINDOW_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/UI/Core/CanvasWindow.h"

namespace Aquila::UI::Core {
class View;
class DockSpace;

class FloatingPanelWindow final : public CanvasWindow {
  public:
	FloatingPanelWindow();
	~FloatingPanelWindow() override;

	void build(Unique<View> panel_subtree, const std::string &title, Uint32 width, Uint32 height, LayoutLoader &loader);

	[[nodiscard]] bool has_content() const;
	Unique<View> detach_content();
	[[nodiscard]] const std::string &get_title() const { return m_title; }
	[[nodiscard]] DockSpace *get_dock_space() const { return m_dock_space; }

  private:
	View *m_body = nullptr;
	DockSpace *m_dock_space = nullptr;
	std::string m_title;
};
}

#endif
