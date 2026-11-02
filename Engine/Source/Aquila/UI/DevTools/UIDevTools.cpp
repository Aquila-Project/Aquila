#include "Aquila/UI/DevTools/UIDevTools.h"

#include "Aquila/Platform/Events/Event.h"
#include "Aquila/Platform/Events/InputEvent.h"
#include "Aquila/Rendering/IRenderWindowHost.h"
#include "Aquila/UI/Core/BehaviourWindow.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/DevTools/PickerOverlay.h"
#include "Aquila/UI/DevTools/UIDebugPanel.h"
#include "Aquila/UI/DevTools/UIDebugWindow.h"
#include "Aquila/UI/DevTools/WidgetGalleryWindow.h"

namespace Aquila::UI::DevTools {

namespace Events = Platform::Events;

namespace {
std::string pick_label(Core::View *view) {
	std::string label(view->get_type_name());
	if (!view->get_id().empty()) {
		label += " #" + view->get_id();
	} else if (!view->get_classes().empty()) {
		label += " ." + view->get_classes().front();
	}
	return label;
}
}

UIDevTools::UIDevTools(const UIDevToolsDesc &desc)
	: m_target(desc.target), m_host(desc.host), m_layouts(desc.layouts) {}

UIDevTools::~UIDevTools() = default;

void UIDevTools::attach(Core::View *layout_root) {
	if (layout_root != nullptr) {
		m_panel = m_layouts.create<UIDebugPanel>("Components/ui_debug_panel", "UIDebugPanel", *layout_root);
		if (m_panel != nullptr) {
			m_panel->set_target(&m_target);
		}
	}

	m_picker = m_target.get_root()->add_child<PickerOverlay>();
}

void UIDevTools::open_inspector_window() {
	if (m_inspector) {
		return; // already open
	}

	constexpr Uint32 width = 800;
	constexpr Uint32 height = 600;

	m_inspector =
		std::make_unique<Core::BehaviourWindow>(m_layouts, std::make_unique<UIDebugWindow>(m_target), width, height);
	if (UIDebugWindow *window = inspector()) {
		window->on_pick_requested = [this] { start_pick(); };
		window->set_ignored_view(m_picker);
		window->on_view_highlighted = [this](Core::View *view) {
			if (m_picker != nullptr && view != nullptr) {
				m_picker->set_target(view->get_absolute_rect(), pick_label(view));
			}
		};
	}

	m_inspector->open(m_host, "Aquila - UI Inspector", [this] {
		stop_pick();
		m_inspector.reset();
	});
}

void UIDevTools::open_widget_gallery() {
	if (m_gallery) {
		return;
	}

	constexpr Uint32 width = 420;
	constexpr Uint32 height = 720;

	m_gallery =
		std::make_unique<Core::BehaviourWindow>(m_layouts, std::make_unique<WidgetGalleryWindow>(), width, height);

	m_gallery->open(m_host, "Aquila - Widget Gallery", [this] { m_gallery.reset(); });
}

UIDebugWindow *UIDevTools::inspector() const {
	return m_inspector ? m_inspector->get_behaviour<UIDebugWindow>() : nullptr;
}

void UIDevTools::start_pick() {
	UIDebugWindow *window = inspector();
	if (window == nullptr) {
		return;
	}
	window->refresh();
	m_pick_hover = nullptr;
	m_pick_mode = true;
}

void UIDevTools::stop_pick() {
	m_pick_mode = false;
	m_pick_hover = nullptr;
	if (m_picker != nullptr) {
		m_picker->clear();
	}
}

bool UIDevTools::on_event(Events::Event &event) {
	if (!m_pick_mode) {
		return false;
	}

	Events::EventDispatcher dispatcher(event);
	bool consumed = false;

	dispatcher.dispatch<Events::MouseMovedEvent>([&](Events::MouseMovedEvent &e) {
		Core::View *hit = m_target.hit_test({ e.get_x(), e.get_y() });
		consumed = true;
		if (hit == m_pick_hover) {
			return true;
		}
		m_pick_hover = hit;
		if (m_picker != nullptr) {
			hit != nullptr ? m_picker->set_target(hit->get_absolute_rect(), pick_label(hit)) : m_picker->clear();
		}
		return true;
	});

	dispatcher.dispatch<Events::MouseButtonPressedEvent>([&](Events::MouseButtonPressedEvent &) {
		Core::View *picked = m_pick_hover;
		stop_pick();
		if (UIDebugWindow *window = inspector(); window != nullptr && picked != nullptr) {
			window->select_view(picked);
		}
		consumed = true;
		return true;
	});

	dispatcher.dispatch<Events::KeyPressedEvent>([&](Events::KeyPressedEvent &e) {
		if (e.get_key_code() == Events::KeyCode::Escape) {
			stop_pick();
		}
		consumed = true;
		return true;
	});

	return consumed;
}

}
