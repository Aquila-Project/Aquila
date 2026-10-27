#include "Aquila/UI/Core/BehaviourWindow.h"

#include "Aquila/Foundation/Log.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/LayoutLoader.h"

namespace Aquila::UI::Core {

BehaviourWindow::BehaviourWindow(LayoutLoader &loader, Unique<WindowBehaviour> window, Uint32 width, Uint32 height) {
	Canvas &canvas = create_canvas(width, height, loader);
	const std::string path = window->get_layout_path();
	WindowBehaviour *behaviour = window.get();
	behaviour->set_overlay_root(canvas.get_root());
	behaviour->set_close_handler([this] { close(); });
	if (loader.build_file(path, *canvas.get_root(), std::move(window)) == nullptr) {
		AQUILA_LOG_ERROR("BehaviourWindow: {} didn't build, the window stays empty", path);
		return;
	}
	m_behaviour = behaviour;
	canvas.reload_styles();
}

BehaviourWindow::~BehaviourWindow() = default;

void BehaviourWindow::on_update(F32 delta_time) {
	if (m_behaviour != nullptr) {
		m_behaviour->update(delta_time);
	}
}

bool BehaviourWindow::intercept_event(Platform::Events::Event &event) {
	return m_behaviour != nullptr && m_behaviour->intercept_event(event);
}

}
