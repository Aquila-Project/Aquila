#include "Core/EditorWindows.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Math/Rect.h"
#include "Aquila/Platform/Input.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/UIResources.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/DockNode.h"
#include "Aquila/UI/Widgets/DockPanel.h"
#include "Aquila/UI/Widgets/DockSpace.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "UI/Components/BuildTemplateInfo.h"
#include "UI/Components/ContentTile.h"
#include "UI/Components/DockTabActions.h"
#include "UI/Components/HierarchyRow.h"
#include "UI/Components/SettingsSection.h"
#include "UI/Components/SettingsShortcutRow.h"
#include "UI/Components/TodoRow.h"

#include <charconv>

namespace Editor {

using namespace Aquila;
using UI::Core::DockNode;
using UI::Core::DockPanel;
using UI::Core::DockSpace;
using UI::Core::View;

EditorWindow::~EditorWindow() {
	release();
	if (m_on_destroyed) {
		m_on_destroyed(this);
	}
}

void EditorWindow::release() {
	if (m_released) {
		return;
	}
	m_released = true;
	run_cleanups();
	for (auto &overlay : m_overlays) {
		if (View *view = overlay.get(); view != nullptr && view->get_parent() != nullptr) {
			view->get_parent()->remove_child(view);
		}
	}
	m_overlays.clear();
}

EditorWindows::EditorWindows(EditorContext &context) : m_context(context) {}

EditorWindows::~EditorWindows() {
	shutdown();
}

void EditorWindows::add(EditorWindowType type) {
	if (find_type(type.id) != nullptr) {
		AQUILA_LOG_WARNING("EditorWindows: window type '{}' is already registered", type.id);
		return;
	}
	m_types.push_back(std::move(type));
}

void EditorWindows::attach(DockSpace &dock_space) {
	m_dock_space = &dock_space;

	if (View *overlay = m_context.overlay_root()) {
		auto menu = std::make_unique<UI::Core::PopupMenu>();
		menu->set_submenu_icon(m_context.icon("chevron-right"));
		m_menu = overlay->add_child(std::move(menu));
		m_staging = overlay->add_child<View>();
		m_staging->set_hidden(true);
	}

	m_component_behaviours.push_back(m_context.layouts().behaviours().add<DockTabActions>("DockTabActions"));
	m_component_behaviours.push_back(m_context.layouts().behaviours().add<HierarchyRow>("HierarchyRow"));
	m_component_behaviours.push_back(m_context.layouts().behaviours().add<ContentTile>("ContentTile"));
	m_component_behaviours.push_back(m_context.layouts().behaviours().add<BuildTemplateInfo>("BuildTemplateInfo"));
	m_component_behaviours.push_back(m_context.layouts().behaviours().add<SettingsSection>("SettingsSection"));
	m_component_behaviours.push_back(m_context.layouts().behaviours().add<SettingsShortcutRow>("SettingsShortcutRow"));
	m_component_behaviours.push_back(m_context.layouts().behaviours().add<TodoRow>("TodoRow"));

	dock_space.set_closable_tabs(true, m_context.icon("x"));
	dock_space.set_panel_factory([this](const std::string &panel_id) { return create_panel(panel_id); });
	dock_space.set_tab_bar_decorator([this](DockNode *node, View *actions) { decorate(node, actions); });
}

void EditorWindows::update(F32 delta_time) {
	std::vector<EditorWindow *> windows;
	windows.reserve(m_live.size());
	for (const Live &live : m_live) {
		windows.push_back(live.window);
	}
	for (EditorWindow *window : windows) {
		window->update(delta_time);
	}
}

void EditorWindows::shutdown() {
	if (m_shut_down) {
		return;
	}
	m_shut_down = true;
	for (const Live &live : m_live) {
		live.window->release();
		live.window->m_on_destroyed = nullptr;
	}
	m_live.clear();
	if (m_dock_space != nullptr) {
		m_dock_space->set_panel_factory(nullptr);
		m_dock_space->set_tab_bar_decorator(nullptr);
	}
}

const EditorWindowType *EditorWindows::find_type(std::string_view type_id) const {
	auto it = std::ranges::find_if(m_types, [type_id](const EditorWindowType &type) { return type.id == type_id; });
	return it != m_types.end() ? &*it : nullptr;
}

const EditorWindows::Live *EditorWindows::find_live(std::string_view type_id) const {
	auto it =
		std::ranges::find_if(m_live, [type_id](const Live &live) { return live.window->get_type_id() == type_id; });
	return it != m_live.end() ? &*it : nullptr;
}

bool EditorWindows::is_open(std::string_view type_id) const {
	return find_live(type_id) != nullptr;
}

bool EditorWindows::is_scope_active(std::string_view type_id) const {
	const Vec2 mouse = Platform::Input::get_mouse_position();
	return std::ranges::any_of(m_live, [type_id, mouse](const Live &live) {
		if (live.window->get_type_id() != type_id || live.panel == nullptr || !live.panel->is_visible()) {
			return false;
		}
		return live.window->accepts_shortcuts(live.panel->get_absolute_rect().contains(mouse));
	});
}

bool EditorWindows::is_visible(std::string_view type_id) const {
	return std::ranges::any_of(m_live, [type_id](const Live &live) {
		if (live.window->get_type_id() != type_id || live.panel == nullptr) {
			return false;
		}
		for (const View *view = live.panel; view != nullptr; view = view->get_parent()) {
			if (view->get_computed_style().display == Aquila::UI::Display::None) {
				return false;
			}
		}
		return true;
	});
}

void EditorWindows::forget(EditorWindow *window) {
	std::erase_if(m_live, [window](const Live &live) { return live.window == window; });
}

Unique<DockPanel> EditorWindows::create_panel(const std::string &panel_id) {
	const std::string type_id = panel_id.substr(0, panel_id.find('#'));
	const EditorWindowType *type = find_type(type_id);
	if (type == nullptr || !type->create) {
		AQUILA_LOG_WARNING("EditorWindows: no window type registered for '{}'", panel_id);
		return nullptr;
	}
	if (type->single_instance && is_open(type_id)) {
		return nullptr;
	}

	std::string instance_id = type_id;
	if (!type->single_instance) {
		const auto hash = panel_id.find('#');
		Uint32 number = 0;
		if (hash != std::string::npos) {
			std::from_chars(panel_id.data() + hash + 1, panel_id.data() + panel_id.size(), number);
		}
		if (number == 0) {
			number = m_next_instance;
		}
		m_next_instance = std::max(m_next_instance, number + 1);
		instance_id = type_id + "#" + std::to_string(number);
	}

	auto created_panel = std::make_unique<DockPanel>(type->title);
	DockPanel *panel = created_panel.get();
	panel->set_id(instance_id);
	panel->set_tab_icon(m_context.icon(type->icon));
	if (m_staging != nullptr) {
		m_staging->add_child(std::move(created_panel));
	}
	const auto unstage = [this, panel, &created_panel] {
		if (m_staging != nullptr) {
			created_panel.reset(static_cast<DockPanel *>(m_staging->detach_child(panel).release()));
		}
		return std::move(created_panel);
	};

	build_into(*panel, *type, type_id);
	return unstage();
}

bool EditorWindows::build_into(DockPanel &panel, const EditorWindowType &type, const std::string &type_id) {
	Unique<EditorWindow> window = type.create(m_context);
	window->m_type_id = type_id;
	window->set_overlay_root(m_context.overlay_root());

	View *overlay = m_context.overlay_root();
	std::vector<View *> before;
	if (overlay != nullptr) {
		for (const auto &child : overlay->get_children()) {
			before.push_back(child.get());
		}
	}

	EditorWindow *built = window.get();
	const std::string path = built->get_layout_path();
	if (m_context.layouts().build_file(path, panel, std::move(window)) == nullptr) {
		AQUILA_LOG_ERROR("EditorWindows: '{}' has no window to show, {} didn't build", type_id, path);
		return false;
	}

	if (overlay != nullptr) {
		std::vector<View *> created;
		for (const auto &child : overlay->get_children()) {
			if (std::ranges::find(before, child.get()) == before.end()) {
				created.push_back(child.get());
			}
		}
		for (View *view : created) {
			built->m_overlays.emplace_back(panel.add_child(overlay->detach_child(view)));
		}
	}

	built->m_on_destroyed = [this](EditorWindow *destroyed) { forget(destroyed); };
	m_live.push_back({ built, &panel });
	return true;
}

void EditorWindows::rebuild_all() {
	std::vector<std::pair<DockPanel *, std::string>> panels;
	for (const Live &live : m_live) {
		panels.emplace_back(live.panel, live.window->get_type_id());
	}
	for (const auto &[panel, type_id] : panels) {
		const EditorWindowType *type = find_type(type_id);
		panel->clear_children();
		if (type != nullptr && type->create) {
			build_into(*panel, *type, type_id);
		}
	}
}

DockNode *EditorWindows::default_target() const {
	if (m_dock_space == nullptr) {
		return nullptr;
	}
	if (DockNode *node = m_dock_space->first_leaf_with_tabs()) {
		return node;
	}
	DockNode *first = nullptr;
	m_dock_space->for_each_leaf([&first](DockNode *leaf) {
		if (first == nullptr) {
			first = leaf;
		}
	});
	return first;
}

namespace {

bool is_leaf_of(const DockSpace &space, const DockNode *node) {
	bool found = false;
	space.for_each_leaf([&](DockNode *leaf) { found = found || leaf == node; });
	return found;
}

} // namespace

EditorWindow *EditorWindows::open(std::string_view type_id, DockNode *target) {
	const EditorWindowType *type = find_type(type_id);
	if (type == nullptr || m_dock_space == nullptr) {
		return nullptr;
	}
	if (target == nullptr || !is_leaf_of(*m_dock_space, target)) {
		target = default_target();
	}

	if (type->single_instance) {
		if (const Live *live = find_live(type_id)) {
			DockPanel *panel = live->panel;
			EditorWindow *window = live->window;
			DockNode *current = m_dock_space->find_node_of(panel);
			if (current != nullptr && target != nullptr && current != target) {
				m_dock_space->move_panel(panel, target);
			}
			if (DockNode *now = m_dock_space->find_node_of(panel)) {
				now->set_active_panel_by_ptr(panel);
			}
			return window;
		}
	}

	if (target == nullptr) {
		return nullptr;
	}

	Unique<DockPanel> panel = create_panel(std::string(type_id));
	if (!panel) {
		return nullptr;
	}
	EditorWindow *window = m_live.back().window;
	std::string title = panel->get_title();
	target->accept_panel(std::move(panel), std::move(title), UI::Core::DropZone::Center);
	return window;
}

void EditorWindows::replace(DockNode *node, std::string_view type_id) {
	if (m_dock_space == nullptr || !is_leaf_of(*m_dock_space, node)) {
		return;
	}
	DockPanel *current = node->get_active_panel_ptr();
	const EditorWindowType *type = find_type(type_id);
	if (current == nullptr || type == nullptr) {
		open(type_id, node);
		return;
	}

	if (type->single_instance) {
		if (const Live *live = find_live(type_id)) {
			DockPanel *existing = live->panel;
			if (existing == current) {
				return;
			}
			DockNode *existing_node = m_dock_space->find_node_of(existing);
			if (existing_node == nullptr) {
				return;
			}
			if (existing_node != node) {
				m_dock_space->move_panel(existing, node);
			}
			node->set_active_panel_by_ptr(existing);
			node->close_panel(current);
			return;
		}
	}

	Unique<DockPanel> created = create_panel(std::string(type_id));
	if (!created) {
		return;
	}
	Unique<View> old = node->replace_panel(current, std::move(created));
	old.reset();
}

void EditorWindows::apply_layout(const UI::Core::DockLayoutDesc &layout) {
	if (m_dock_space != nullptr) {
		m_dock_space->apply_layout(layout, true);
	}
}

UI::Core::DockLayoutDesc EditorWindows::capture() const {
	return m_dock_space != nullptr ? UI::Core::DockLayoutSerializer::capture(*m_dock_space)
								   : UI::Core::DockLayoutDesc{};
}

void EditorWindows::decorate(DockNode *node, View *actions) {
	auto *buttons = m_context.layouts().create<DockTabActions>(UI::Core::engine_component_file("dock_tab_actions"),
															   "DockTabActions", *actions);
	if (buttons == nullptr) {
		return;
	}
	constexpr F32 K_MENU_GAP = 2.F;
	buttons->on_change_editor.connect(
		[this, node](Vec2 corner) { open_type_menu({ corner.x, corner.y + K_MENU_GAP }, true, node); });
	buttons->on_add_tab.connect(
		[this, node](Vec2 corner) { open_type_menu({ corner.x, corner.y + K_MENU_GAP }, false, node); });
}

void EditorWindows::open_type_menu(Vec2 position, bool replace, DockNode *node) {
	if (m_menu == nullptr) {
		return;
	}
	m_menu->clear_items();

	std::vector<std::string> groups;
	for (const EditorWindowType &type : m_types) {
		if (std::ranges::find(groups, type.group) == groups.end()) {
			groups.push_back(type.group);
		}
	}

	for (Usize g = 0; g < groups.size(); ++g) {
		if (g > 0) {
			m_menu->add_separator();
		}
		for (const EditorWindowType &type : m_types) {
			if (type.group != groups[g]) {
				continue;
			}
			std::string hint = (type.single_instance && is_open(type.id)) ? "open" : std::string{};
			m_menu->add_item(type.title, std::move(hint), m_context.icon(type.icon),
							 [this, id = type.id, replace, node] {
								 if (replace) {
									 this->replace(node, id);
								 } else {
									 open(id, node);
								 }
							 });
		}
	}
	m_menu->open_at(position);
}

} // namespace Editor
