#ifndef AQUILA_UI_CORE_DOCK_WINDOW_MANAGER_H
#define AQUILA_UI_CORE_DOCK_WINDOW_MANAGER_H

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Rendering/IRenderWindowHost.h"
#include "Aquila/UI/Core/FloatingPanelWindow.h"

namespace Aquila::UI::Core {

class Canvas;
class DockSpace;
class View;

/// Tear-off: dragging a tab out of a dock space opens it in its own OS window,
/// and dragging that window over a dock space docks it back.
class DockWindowManager {
  public:
	DockWindowManager(Aquila::Rendering::IRenderWindowHost &host, std::vector<std::string> style_paths);
	~DockWindowManager();

	AQUILA_NONCOPYABLE(DockWindowManager);
	AQUILA_NONMOVEABLE(DockWindowManager);

	void set_main_dock_space(DockSpace *dock_space);

	[[nodiscard]] Canvas *find_canvas(Aquila::Rendering::RenderWindowId window) const;
	void reload_styles();

  private:
	struct FloatingEntry {
		Unique<FloatingPanelWindow> panel;
		Aquila::Rendering::RenderWindowId window = nullptr;
	};

	void wire_dock_space(DockSpace *dock_space, Aquila::Rendering::RenderWindowId source);
	void handle_tear_off(Aquila::Rendering::RenderWindowId source, Unique<View> content, std::string title, Vec2 source_local);
	void preview_dock_targets(Aquila::Rendering::RenderWindowId source, Vec2 source_local);
	void clear_dock_target_previews();
	DockSpace *find_dock_target_at_screen(Vec2 screen_pos, Aquila::Rendering::RenderWindowId exclude, Vec2 &out_local);
	void close_floating_window(Aquila::Rendering::RenderWindowId window);
	void spawn_floating_panel(Unique<View> panel_subtree, std::string title, Vec2 screen_pos);
	void on_floating_closed(FloatingPanelWindow *panel);
	void dock_back_to_center(Unique<View> content, const std::string &title);

	Aquila::Rendering::IRenderWindowHost &m_host;
	std::vector<std::string> m_style_paths;
	DockSpace *m_main_dock_space = nullptr;
	std::vector<FloatingEntry> m_floating_panels;
};

}

#endif
