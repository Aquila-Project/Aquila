#ifndef AQUILA_UI_DEVTOOLS_WIDGET_GALLERY_WINDOW_H
#define AQUILA_UI_DEVTOOLS_WIDGET_GALLERY_WINDOW_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Graphics {
class Renderer2D;
}
namespace Aquila::GFX {
class GfxCommandList;
class GfxContext;
}
namespace Aquila::Platform::Events {
class Event;
}
namespace Aquila::UI::Core {
class Canvas;
class View;
class Tooltip;
class TextureCache;
} // namespace Aquila::UI::Core

namespace Aquila::UI::DevTools {

/// Every widget on one page. Meant for open_content_window().
class WidgetGalleryWindow {
  public:
	WidgetGalleryWindow();
	~WidgetGalleryWindow();

	void build(Aquila::GFX::GfxContext &ctx, Aquila::UI::Core::TextureCache *texture_cache, Uint32 width, Uint32 height,
			   const std::vector<std::string> &style_paths);

	void update(F32 delta_time);
	void render(Aquila::Graphics::Renderer2D &batcher, Aquila::GFX::GfxCommandList &cmd);
	void on_event(Aquila::Platform::Events::Event &event);
	void set_frame_target(const void *target);

  private:
	Aquila::UI::Core::View *add_group(Aquila::UI::Core::View *host, const std::string &heading);

	Unique<Aquila::UI::Core::Canvas> m_canvas;
	Aquila::UI::Core::Tooltip *m_tooltip = nullptr;
};

} // namespace Aquila::UI::DevTools

#endif
