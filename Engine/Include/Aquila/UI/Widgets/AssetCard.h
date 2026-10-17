#pragma once

#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/AssetPayload.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Core/DragState.h"

namespace Aquila::UI::Core {

/// One file in the content browser: thumbnail, type and name. Dragging it drags its AssetPayload.
class AssetCard : public View {
  public:
	AssetCard();

	[[nodiscard]] std::string_view get_type_name() const override { return "AssetCard"; }

	void set_asset(AssetPayload payload);
	[[nodiscard]] const AssetPayload &get_asset() const { return m_payload; }

	void set_thumbnail(GFX::GfxTexture *texture);

	void on_drag_start(DragState &state) override;

  private:
	AssetPayload m_payload;

	Image *m_thumbnail = nullptr;
	Label *m_type_label = nullptr;
	Label *m_name_label = nullptr;
};

} // namespace Aquila::UI::Core
