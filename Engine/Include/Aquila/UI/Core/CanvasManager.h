#pragma once

#include "Aquila/Graphics/Core/Renderer2D.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/Foundation/Singleton.h"

namespace Aquila::UI::Core {

/// Drawn back to front in this order. Each layer is its own Canvas.
enum class UILayer : Uint8 {
	WorldSpace = 0,
	ScreenCamera = 1,
	Editor = 2,
	ScreenOverlay = 3,
	Count // keep track of how many layers we have
};

class CanvasManager : public Foundation::Singleton<CanvasManager> {
  public:
	Canvas &get_layer(UILayer layer);

	void on_event(Platform::Events::Event &e);
	void update(float delta_time);
	void compute();

	void render_layers(Graphics::Renderer2D &r2d, GFX::GfxCommandList &cmd, UILayer from, UILayer to);
	void resize_layers(UILayer from, UILayer to, Uint32 width, Uint32 height);

	bool is_any_layer_dirty(UILayer from, UILayer to) const;
	void clear_layer_dirty_flags(UILayer from, UILayer to);

	/// The cursor of the top-most hovered view.
	[[nodiscard]] Platform::CursorType get_active_cursor() const {
		for (size_t i = static_cast<size_t>(UILayer::Count); i-- > 0;) {
			if (View *hovered = m_layers[i]->get_hovered_view()) {
				return hovered->get_cursor();
			}
		}
		return Platform::CursorType::Arrow;
	}

  private:
	friend class Foundation::Singleton<CanvasManager>;
	CanvasManager(Uint32 width, Uint32 height);

	std::array<Unique<Canvas>, static_cast<size_t>(UILayer::Count)> m_layers;
};

} // namespace Aquila::UI::Core
