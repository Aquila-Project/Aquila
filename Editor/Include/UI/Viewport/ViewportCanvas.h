#pragma once

#include "Aquila/UI/Core/View.h"

namespace Editor {

/// Base for 2D drawn over the viewport. It covers the viewport and clips to it through an inner layer.
class ViewportCanvas : public Aquila::UI::Core::View {
  public:
	explicit ViewportCanvas(Int16 z_index);

	/// Called when the canvas redraws, after redraw().
	virtual void draw(Aquila::UI::Rendering::DrawList &draw_list) const = 0;

  protected:
	/// Call it with the viewport rect whenever it can change, it only does work when the rect moved.
	void fit(const Rect &viewport);
	void redraw();
	[[nodiscard]] Aquila::UI::Text::FontAtlas *font() const;
	[[nodiscard]] F32 font_size() const;

  private:
	Aquila::UI::Core::View *m_layer = nullptr;
};

}
