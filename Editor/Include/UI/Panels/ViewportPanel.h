#pragma once

#include "UI/Panels/IEditorPanel.h"
#include "Aquila/Foundation/Math/Rect.h"
#include "Aquila/Foundation/Signal.h"

namespace Aquila::GFX {
class GfxTexture;
}

namespace Aquila::UI::Core {
class Image;
class View;
}

namespace Editor {

class ViewportPanel : public IEditorPanel {
  public:
	explicit ViewportPanel(Aquila::GFX::GfxTexture &initial_texture);
	void build(Aquila::UI::Core::View *panel, Aquila::UI::Core::View *overlay_root) override;
	void set_texture(Aquila::GFX::GfxTexture *texture);

	[[nodiscard]] Rect get_content_rect() const;
	[[nodiscard]] const Aquila::UI::Core::View *get_view() const;

	/// Where the image was clicked, 0..1 on both axes.
	Signal<void(Vec2)> on_clicked_uv;

  private:
	Aquila::GFX::GfxTexture &m_initial_texture;
	Aquila::UI::Core::Image *m_image = nullptr;
};

} // namespace Editor
