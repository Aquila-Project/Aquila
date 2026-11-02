#ifndef AQUILA_UI_DEVTOOLS_WIDGET_GALLERY_WINDOW_H
#define AQUILA_UI_DEVTOOLS_WIDGET_GALLERY_WINDOW_H

#include "Aquila/UI/Core/WindowBehaviour.h"

namespace Aquila::UI::DevTools {

/// Every widget on one page.
class WidgetGalleryWindow final : public Aquila::UI::Core::WindowBehaviour {
  public:
	WidgetGalleryWindow();

	void define_behaviour() override;
};

}

#endif
