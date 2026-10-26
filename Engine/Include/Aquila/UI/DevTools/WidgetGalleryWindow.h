#ifndef AQUILA_UI_DEVTOOLS_WIDGET_GALLERY_WINDOW_H
#define AQUILA_UI_DEVTOOLS_WIDGET_GALLERY_WINDOW_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/UI/Core/CanvasWindow.h"

namespace Aquila::GFX {
class GfxContext;
}
namespace Aquila::UI::Core {
class View;
class Tooltip;
class TextureCache;
} // namespace Aquila::UI::Core

namespace Aquila::UI::DevTools {

/// Every widget on one page.
class WidgetGalleryWindow final : public Aquila::UI::Core::CanvasWindow {
  public:
	WidgetGalleryWindow();
	~WidgetGalleryWindow() override;

	void build(Aquila::GFX::GfxContext &ctx, Aquila::UI::Core::TextureCache *texture_cache, Uint32 width, Uint32 height,
			   Aquila::UI::Core::LayoutLoader &loader);

  private:
	Aquila::UI::Core::View *add_group(Aquila::UI::Core::View *host, const std::string &heading);

	Aquila::UI::Core::Tooltip *m_tooltip = nullptr;
};

} // namespace Aquila::UI::DevTools

#endif
