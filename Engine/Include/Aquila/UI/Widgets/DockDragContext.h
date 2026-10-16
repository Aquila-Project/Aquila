#pragma once

#include "Aquila/UI/Core/View.h"

namespace Aquila::GFX {
class GfxTexture;
}

namespace Aquila::UI::Core {

class DockNode;
class DockPanel;

struct DockDragContext {
	bool active = false;
	std::string title = "";
	DockPanel *panel = nullptr;
	DockNode *source_node = nullptr;

	Unique<View> external_view;

	std::function<void(Vec2)> on_move;
	std::function<void(Vec2)> on_release;
	std::function<void(DockNode *)> on_node_emptied;

	std::function<void(DockNode *, View *)> decorate_tab_bar;
	GFX::GfxTexture *close_icon = nullptr;
	bool closable_tabs = false;
};

} // namespace Aquila::UI::Core
