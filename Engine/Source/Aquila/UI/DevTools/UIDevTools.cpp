#include "Aquila/UI/DevTools/UIDevTools.h"

#include "Aquila/Platform/Events/Event.h"
#include "Aquila/Platform/Events/InputEvent.h"
#include "Aquila/Rendering/IRenderWindowHost.h"
#include "Aquila/UI/Core/Canvas.h"
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
	m_panel = std::make_unique<UIDebugPanel>();
	m_panel->build(layout_root, &m_target);

	m_picker = m_target.get_root()->add_child<PickerOverlay>();
}

void UIDevTools::open_inspector_window() {
	if (m_inspector) {
		return; // already open
	}

	constexpr Uint32 width = 800;
	constexpr Uint32 height = 600;

	m_inspector = std::make_unique<UIDebugWindow>();
	m_inspector->build(&m_target, width, height, m_layouts);

	m_inspector->on_pick_requested = [this] { start_pick(); };
	m_inspector->set_ignored_view(m_picker);
	m_inspector->on_view_highlighted = [this](Core::View *view) {
		if (m_picker != nullptr && view != nullptr) {
			m_picker->set_target(view->get_absolute_rect(), pick_label(view));
		}
	};

	m_inspector->open(m_host, "Aquila - UI Inspector", [this] {
		stop_pick();
		m_inspector.reset();
	});
}

void UIDevTools::open_widget_gallery(GFX::GfxContext &ctx, Core::TextureCache *texture_cache) {
	if (m_gallery) {
		return;
	}

	constexpr Uint32 width = 420;
	constexpr Uint32 height = 720;

	m_gallery = std::make_unique<WidgetGalleryWindow>();
	m_gallery->build(ctx, texture_cache, width, height, m_layouts);

	m_gallery->open(m_host, "Aquila - Widget Gallery", [this] { m_gallery.reset(); });
}

void UIDevTools::start_pick() {
	if (!m_inspector) {
		return;
	}
	m_inspector->refresh();
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
		if (m_inspector && picked != nullptr) {
			m_inspector->select_view(picked);
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
