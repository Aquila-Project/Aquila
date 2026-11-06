#include "Aquila/UI/Core/UIHost.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Platform/Events/Event.h"
#include "Aquila/Rendering/Renderers/OverlayRenderer.h"
#include "Aquila/UI/Core/Clipboard.h"
#include "Aquila/UI/Rendering/ViewRenderingSystem.h"
#include "Aquila/UI/Widgets/AssetSlot.h"

namespace Aquila::UI::Core {

UIHost::UIHost(const UIHostDesc &desc) {
	m_layouts.styles().set_base(desc.base_styles);
	CanvasManager::init(desc.width, desc.height);
	for (Uint8 layer = 0; layer < static_cast<Uint8>(UILayer::Count); ++layer) {
		CanvasManager::get()->get_layer(static_cast<UILayer>(layer)).set_layout_loader(&m_layouts);
	}
	desc.overlay_renderer.add_system<Rendering::ViewRenderingSystem>();
	Clipboard::init(desc.clipboard_get, desc.clipboard_set);
	if (Foundation::FrameScheduler::is_initialized()) {
		m_missing_poll = Foundation::FrameScheduler::get()->add_poll([] { AssetSlot::check_missing(); });
	}
}

UIHost::~UIHost() {
	CanvasManager::shutdown();
}

void UIHost::update(F32 delta_time) {
	CanvasManager::get()->update(delta_time);
	CanvasManager::get()->compute();
}

void UIHost::on_event(Platform::Events::Event &event) {
	CanvasManager::get()->on_event(event);
}

}
