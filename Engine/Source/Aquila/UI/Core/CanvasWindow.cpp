#include "Aquila/UI/Core/CanvasWindow.h"

#include "Aquila/Platform/Events/Event.h"
#include "Aquila/Platform/Events/WindowEvent.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Core/LayoutLoader.h"

namespace Aquila::UI::Core {

namespace Events = Platform::Events;

CanvasWindow::CanvasWindow() = default;
CanvasWindow::~CanvasWindow() = default;

Aquila::Rendering::RenderWindowId CanvasWindow::open(Aquila::Rendering::IRenderWindowHost &host,
													 const std::string &title, Delegate<void()> on_closed,
													 Vec2 min_size) {
	m_host = &host;
	m_window = Aquila::Rendering::open_content_window(host, *this, m_canvas->get_width(), m_canvas->get_height(), title,
													  [this, on_closed = std::move(on_closed)] {
														  m_window = nullptr;
														  if (on_closed) {
															  on_closed();
														  }
													  });
	if (min_size.x > 0.F || min_size.y > 0.F) {
		host.set_window_min_size(m_window, min_size);
	}
	return m_window;
}

void CanvasWindow::close() {
	if (m_host != nullptr && m_window != nullptr) {
		m_host->request_close(m_window);
	}
}

Canvas &CanvasWindow::create_canvas(Uint32 width, Uint32 height, LayoutLoader &loader) {
	m_canvas = std::make_unique<Canvas>(width, height);
	m_canvas->set_layout_loader(&loader);
	return *m_canvas;
}

void CanvasWindow::update(F32 delta_time) {
	on_update(delta_time);
	m_canvas->update(delta_time);
	m_canvas->compute();
}

void CanvasWindow::render(Graphics::Renderer2D &batcher, GFX::GfxCommandList &cmd) {
	m_canvas->submit_to_quad_batcher(batcher, cmd);
}

void CanvasWindow::on_event(Events::Event &event) {
	Events::EventDispatcher dispatcher(event);
	dispatcher.dispatch<Events::WindowResizeEvent>([this](Events::WindowResizeEvent &e) {
		if (e.get_width() > 0 && e.get_height() > 0) {
			m_canvas->resize(e.get_width(), e.get_height());
		}
		return false;
	});

	if (!intercept_event(event)) {
		m_canvas->on_event(event);
	}
}

void CanvasWindow::set_frame_target(const void *target) {
	m_canvas->set_frame_target(target);
}

Platform::CursorType CanvasWindow::get_cursor() const {
	const View *hovered = m_canvas->get_hovered_view();
	return hovered != nullptr ? hovered->get_cursor() : Platform::CursorType::Arrow;
}

}
