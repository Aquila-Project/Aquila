#include "Aquila/UI/Core/BehaviourWindow.h"

#include "Aquila/Foundation/Log.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/View.h"

namespace Aquila::UI::Core {

BehaviourWindow::BehaviourWindow(LayoutLoader &loader, Unique<WindowBehaviour> window, Uint32 width, Uint32 height)
	: m_loader(loader) {
	create_canvas(width, height, loader);
	build(std::move(window));
}

BehaviourWindow::BehaviourWindow(LayoutLoader &loader, Factory factory, Uint32 width, Uint32 height)
	: m_loader(loader), m_factory(std::move(factory)) {
	create_canvas(width, height, loader);
	if (m_factory) {
		build(m_factory());
	}
}

void BehaviourWindow::build(Unique<WindowBehaviour> window) {
	Canvas *canvas = get_canvas();
	if (canvas == nullptr || window == nullptr) {
		return;
	}
	const std::string path = window->get_layout_path();
	WindowBehaviour *behaviour = window.get();
	behaviour->set_overlay_root(canvas->get_root());
	behaviour->set_close_handler([this] { close(); });
	if (m_loader.build_file(path, *canvas->get_root(), std::move(window)) == nullptr) {
		AQUILA_LOG_ERROR("BehaviourWindow: {} didn't build, the window stays empty", path);
		return;
	}
	m_behaviour = behaviour;
	canvas->reload_styles();
}

bool BehaviourWindow::rebuild() {
	Canvas *canvas = get_canvas();
	if (!m_factory || canvas == nullptr) {
		return false;
	}
	Unique<WindowBehaviour> next = m_factory();
	if (next == nullptr) {
		return false;
	}
	if (m_behaviour != nullptr) {
		next->take_state(*m_behaviour);
	}
	m_behaviour = nullptr;
	canvas->get_root()->clear_children();
	build(std::move(next));
	return m_behaviour != nullptr;
}

BehaviourWindow::~BehaviourWindow() = default;

void BehaviourWindow::on_update(F32 delta_time) {
	if (m_behaviour != nullptr) {
		m_behaviour->update(delta_time);
	}
}

bool BehaviourWindow::intercept_event(Platform::Events::Event &event) {
	if (m_event_hook && m_event_hook(event)) {
		return true;
	}
	return m_behaviour != nullptr && m_behaviour->intercept_event(event);
}

}
