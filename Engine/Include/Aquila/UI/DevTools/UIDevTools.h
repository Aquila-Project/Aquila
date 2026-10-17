#ifndef AQUILA_UI_DEVTOOLS_UI_DEV_TOOLS_H
#define AQUILA_UI_DEVTOOLS_UI_DEV_TOOLS_H

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/UI/Core/View.h"

namespace Aquila::GFX {
class GfxContext;
}

namespace Aquila::Platform::Events {
class Event;
}

namespace Aquila::Rendering {
class IRenderWindowHost;
}

namespace Aquila::UI::Core {
class Canvas;
class TextureCache;
}

namespace Aquila::UI::DevTools {

class PickerOverlay;
class UIDebugPanel;
class UIDebugWindow;
class WidgetGalleryWindow;

struct UIDevToolsDesc {
	Core::Canvas &target;
	Aquila::Rendering::IRenderWindowHost &host;
	std::vector<std::string> style_paths;
};

/// The UI inspector, the element picker and the widget gallery for one canvas.
class UIDevTools {
  public:
	explicit UIDevTools(const UIDevToolsDesc &desc);
	~UIDevTools();

	AQUILA_NONCOPYABLE(UIDevTools);
	AQUILA_NONMOVEABLE(UIDevTools);

	void attach(Core::View *layout_root);

	void open_inspector_window();
	void open_widget_gallery(GFX::GfxContext &ctx, Core::TextureCache *texture_cache);

	/// @return true when it used the event, while picking an element
	bool on_event(Platform::Events::Event &event);

  private:
	void start_pick();
	void stop_pick();

	Core::Canvas &m_target;
	Aquila::Rendering::IRenderWindowHost &m_host;
	std::vector<std::string> m_style_paths;

	Unique<UIDebugPanel> m_panel;
	Unique<UIDebugWindow> m_inspector;
	Unique<WidgetGalleryWindow> m_gallery;
	PickerOverlay *m_picker = nullptr;

	Core::ViewRef m_pick_hover;
	bool m_pick_mode = false;
};

}

#endif
