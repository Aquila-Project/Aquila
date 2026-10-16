#include "Aquila/UI/Core/DockWindowManager.h"

#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Widgets/DockNode.h"
#include "Aquila/UI/Widgets/DockSpace.h"
#include "Aquila/UI/Widgets/DockTypes.h"

namespace Aquila::UI::Core {

namespace {
constexpr Uint32 k_floating_width = 800;
constexpr Uint32 k_floating_height = 600;
constexpr Vec2 k_floating_cursor_offset = { 60.F, 12.F };

bool contains_local(Vec2 local, Vec2 size) {
	return local.x >= 0.F && local.y >= 0.F && local.x < size.x && local.y < size.y;
}
}

DockWindowManager::DockWindowManager(Aquila::Rendering::IRenderWindowHost &host, std::vector<std::string> style_paths)
	: m_host(host), m_style_paths(std::move(style_paths)) {}

DockWindowManager::~DockWindowManager() = default;

void DockWindowManager::set_main_dock_space(DockSpace *dock_space) {
	m_main_dock_space = dock_space;
	wire_dock_space(dock_space, m_host.get_main_window());
}

Canvas *DockWindowManager::find_canvas(Aquila::Rendering::RenderWindowId window) const {
	for (const auto &entry : m_floating_panels) {
		if (entry.window == window) {
			return entry.panel->get_canvas();
		}
	}
	return nullptr;
}

void DockWindowManager::reload_styles() {
	for (const auto &entry : m_floating_panels) {
		Canvas *canvas = entry.panel->get_canvas();
		StyleParser::load_files(m_style_paths, canvas->get_style_sheet());
		canvas->reload_styles();
	}
}

void DockWindowManager::wire_dock_space(DockSpace *dock_space, Aquila::Rendering::RenderWindowId source) {
	dock_space->set_tear_off_callback([this, source](Unique<View> sub, std::string title, Vec2 pos) {
		handle_tear_off(source, std::move(sub), std::move(title), pos);
	});
	dock_space->set_external_drag_observer([this, source](Vec2 pos) { preview_dock_targets(source, pos); },
										   [this] { clear_dock_target_previews(); });
	dock_space->set_emptied_callback([this, source] { close_floating_window(source); });
}

void DockWindowManager::close_floating_window(Aquila::Rendering::RenderWindowId window) {
	for (auto &entry : m_floating_panels) {
		if (entry.window == window) {
			m_host.request_close(window);
			return;
		}
	}
}

DockSpace *DockWindowManager::find_dock_target_at_screen(Vec2 screen_pos, Aquila::Rendering::RenderWindowId exclude,
														 Vec2 &out_local) {
	struct Candidate {
		DockSpace *dock_space;
		Aquila::Rendering::RenderWindowId window;
	};
	std::vector<Candidate> candidates;
	candidates.reserve(m_floating_panels.size() + 1);
	for (auto &entry : m_floating_panels) {
		candidates.push_back({ entry.panel->get_dock_space(), entry.window });
	}
	candidates.push_back({ m_main_dock_space, m_host.get_main_window() });

	for (auto &c : candidates) {
		if (c.window == exclude || c.dock_space == nullptr) {
			continue;
		}
		const Vec2 local = screen_pos - m_host.get_window_position(c.window);
		if (contains_local(local, m_host.get_window_size(c.window))) {
			out_local = local;
			return c.dock_space;
		}
	}
	return nullptr;
}

void DockWindowManager::preview_dock_targets(Aquila::Rendering::RenderWindowId source, Vec2 source_local) {
	const Vec2 screen = m_host.get_window_position(source) + source_local;

	clear_dock_target_previews();

	Vec2 target_local{ 0.F, 0.F };
	if (DockSpace *target = find_dock_target_at_screen(screen, source, target_local)) {
		target->preview_external_drag(target_local);
	}
}

void DockWindowManager::clear_dock_target_previews() {
	if (m_main_dock_space != nullptr) {
		m_main_dock_space->clear_external_drag();
	}
	for (auto &entry : m_floating_panels) {
		entry.panel->get_dock_space()->clear_external_drag();
	}
}

void DockWindowManager::handle_tear_off(Aquila::Rendering::RenderWindowId source, Unique<View> content, std::string title,
										Vec2 source_local) {
	clear_dock_target_previews();

	const Vec2 screen = m_host.get_window_position(source) + source_local;

	// A release inside the source window's own bounds floats the panel — the source sits on top
	const bool inside_source = contains_local(source_local, m_host.get_window_size(source));

	Vec2 target_local{ 0.F, 0.F };
	DockSpace *target = inside_source ? nullptr : find_dock_target_at_screen(screen, source, target_local);
	const bool docked = (target != nullptr) && target->try_dock_external(content, title, target_local);

	if (!docked) {
		spawn_floating_panel(std::move(content), std::move(title), screen);
	}

	// A floating window drained of its last tab has nothing left to show — close it.
	for (auto &entry : m_floating_panels) {
		if (entry.window == source && !entry.panel->has_content()) {
			close_floating_window(source);
			break;
		}
	}
}

void DockWindowManager::spawn_floating_panel(Unique<View> panel_subtree, std::string title, Vec2 screen_pos) {
	auto floating = std::make_unique<FloatingPanelWindow>();
	floating->build(std::move(panel_subtree), title, k_floating_width, k_floating_height, m_style_paths);
	FloatingPanelWindow *panel = floating.get();

	Aquila::Rendering::RenderWindowCallbacks callbacks;
	callbacks.on_update = [panel](F32 dt) { panel->update(dt); };
	callbacks.on_render = [panel](Graphics::Renderer2D &batcher, GFX::GfxCommandList &cmd) {
		panel->render(batcher, cmd);
	};
	callbacks.on_event = [panel](Platform::Events::Event &event) { panel->on_event(event); };
	callbacks.on_close = [this, panel] { on_floating_closed(panel); };
	callbacks.cursor = [panel] { return panel->get_cursor(); };
	callbacks.forward_events = true;

	const Aquila::Rendering::RenderWindowId window =
		m_host.create_window(k_floating_width, k_floating_height, title, std::move(callbacks));
	m_host.set_window_position(window, screen_pos - k_floating_cursor_offset);

	wire_dock_space(panel->get_dock_space(), window);

	m_floating_panels.push_back({ std::move(floating), window });
}

void DockWindowManager::on_floating_closed(FloatingPanelWindow *panel) {
	auto it =
		std::ranges::find_if(m_floating_panels, [panel](const FloatingEntry &e) { return e.panel.get() == panel; });
	if (it == m_floating_panels.end()) {
		return;
	}

	while (panel->has_content()) {
		auto content = panel->detach_content();
		if (!content) {
			break;
		}
		dock_back_to_center(std::move(content), panel->get_title());
	}

	m_floating_panels.erase(it);
}

void DockWindowManager::dock_back_to_center(Unique<View> content, const std::string &title) {
	if (!content || (m_main_dock_space == nullptr)) {
		return;
	}

	DockNode *node =
		m_main_dock_space->get_root_node()->hit_test_node(m_main_dock_space->get_absolute_rect().center());
	if (node == nullptr) {
		return;
	}
	node->accept_panel(std::move(content), title, DropZone::Center);
}

}
