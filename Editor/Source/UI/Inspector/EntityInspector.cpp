#include "UI/Inspector/EntityInspector.h"

#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/Platform/Input.h"
#include "Aquila/Scene/ComponentRegistry.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Core/ComponentEditors.h"
#include "Core/EditorContext.h"

namespace Editor {

using Aquila::SceneManagement::ComponentCategory;
using Aquila::SceneManagement::ComponentDescriptor;
using Aquila::SceneManagement::Entity;
using Aquila::UI::Core::Inspectable;
using Aquila::UI::Core::InspectorSection;
using Aquila::UI::Core::PopupMenu;

namespace {

constexpr Usize K_OBJECT_TAB = 0;
constexpr Usize K_COMPONENTS_TAB = 1;
constexpr Usize K_RENDERING_TAB = 2;

Usize tab_of(ComponentCategory category) {
	switch (category) {
	case ComponentCategory::Object:
		return K_OBJECT_TAB;
	case ComponentCategory::Rendering:
		return K_RENDERING_TAB;
	case ComponentCategory::General:
		break;
	}
	return K_COMPONENTS_TAB;
}

}

EntityInspector::EntityInspector(EditorContext &context) : m_context(context) {
	Aquila::UI::Core::View *overlay = m_context.overlay_root();
	if (overlay == nullptr) {
		return;
	}
	m_add_menu = overlay->add_child(std::make_unique<PopupMenu>());
	m_add_menu->set_submenu_icon(m_context.icon("chevron-right"));
	m_add_search = m_add_menu->enable_search("Search components...");
	m_add_search->on_changed.connect([this](const std::string &query) { populate_add_menu(query); });
}

EntityInspector::~EntityInspector() = default;

Inspectable EntityInspector::inspect(Entity entity) {
	m_entity = entity;
	const ComponentEditors &editors = m_context.component_editors;

	Inspectable inspectable;
	inspectable.icon = editors.entity_icon(entity);
	inspectable.title = entity.get_name();
	inspectable.rename = [this, entity](const std::string &name) {
		if (!entity.exists()) {
			return;
		}
		Entity target = entity;
		target.set_name(name);
		m_context.selection.on_entity_modified(target);
	};
	const std::string uuid = entity.get_uuid().to_string();
	inspectable.subtitle = uuid.substr(0, 8);
	inspectable.subtitle_tooltip = uuid;

	inspectable.tabs.resize(3);
	inspectable.tabs[K_OBJECT_TAB] = { .title = "Object", .icon = "square", .always_visible = true };
	inspectable.tabs[K_COMPONENTS_TAB] = { .title = "Components", .icon = "layers", .always_visible = true };
	inspectable.tabs[K_RENDERING_TAB] = { .title = "Rendering", .icon = "camera" };
	for (const ComponentDescriptor *descriptor : editors.ordered()) {
		if (descriptor->has(entity)) {
			inspectable.tabs[tab_of(descriptor->get_category())].sections.push_back(make_section(entity, *descriptor));
		}
	}
	inspectable.tabs[K_COMPONENTS_TAB].actions.push_back({
		.text = "Add Component",
		.icon = "plus",
		.run = [this] { open_add_menu(Aquila::Platform::Input::get_mouse_position(), false); },
	});
	return inspectable;
}

InspectorSection EntityInspector::make_section(Entity entity, const ComponentDescriptor &d) {
	InspectorSection section;
	section.id = d.get_name();
	section.title = d.get_name();
	section.icon = m_context.component_editors.icon(d.get_name());
	section.type = &d.get_type();
	section.instance = [&d, entity]() -> void * { return entity.exists() ? d.get(entity) : nullptr; };
	section.on_edited = [this, &d, entity] {
		if (!entity.exists()) {
			return;
		}
		if (Signal<void()> *changed = d.get_changed_signal(entity)) {
			(*changed)();
		}
		m_context.selection.on_entity_modified(entity);
	};
	if (const ComponentEditor *editor = m_context.component_editors.find(d.get_name());
		editor != nullptr && editor->content) {
		section.content = editor->content(entity, m_context);
	}
	if (d.is_removable()) {
		section.header_actions.push_back({
			.icon = "x",
			.tooltip = "Remove " + d.get_name(),
			.run = [this, &d, entity] { remove_component(entity, d); },
		});
	}
	return section;
}

void EntityInspector::add_component(Entity entity, const ComponentDescriptor &d) {
	if (!entity.exists()) {
		return;
	}
	const ComponentEditor *editor = m_context.component_editors.find(d.get_name());
	if (editor != nullptr && editor->add) {
		editor->add(entity, m_context);
	} else {
		d.add(entity);
	}
	AQUILA_LOG_INFO("Added {} component to '{}'", d.get_name(), entity.get_name());
	m_context.selection.on_entity_modified(entity);
	m_context.selection.on_components_changed(entity);
}

void EntityInspector::remove_component(Entity entity, const ComponentDescriptor &d) {
	if (!entity.exists()) {
		return;
	}
	d.remove(entity);
	m_context.selection.on_entity_modified(entity);
	m_context.selection.on_components_changed(entity);
}

void EntityInspector::open_add_menu(Vec2 position, bool from_keyboard) {
	if (m_add_menu == nullptr || !m_entity.exists()) {
		return;
	}
	if (m_add_search != nullptr) {
		m_add_search->set_text("");
	}
	populate_add_menu("");
	m_add_menu->open_at(position);
	if (m_add_search != nullptr) {
		m_add_search->request_focus();
		if (from_keyboard) {
			m_add_search->ignore_next_char();
		}
	}
}

void EntityInspector::populate_add_menu(const std::string &query) {
	if (m_add_menu == nullptr) {
		return;
	}
	m_add_menu->clear_items();
	if (!m_entity.exists()) {
		m_add_menu->refresh();
		return;
	}

	const ComponentEditors &editors = m_context.component_editors;
	std::vector<const ComponentDescriptor *> addable;
	for (const ComponentDescriptor *descriptor : editors.ordered()) {
		if (descriptor->can_add() && descriptor->is_removable() && !descriptor->has(m_entity)) {
			addable.push_back(descriptor);
		}
	}
	const auto add_item = [this, &editors](PopupMenu *menu, const ComponentDescriptor *descriptor) {
		menu->add_item(descriptor->get_name(), {}, m_context.icon(editors.icon(descriptor->get_name())),
					   [this, descriptor] { add_component(m_entity, *descriptor); });
	};

	const std::string needle = Aquila::Foundation::to_lower(query);
	if (!needle.empty()) {
		for (const ComponentDescriptor *descriptor : addable) {
			if (Aquila::Foundation::to_lower(descriptor->get_name()).find(needle) != std::string::npos) {
				add_item(m_add_menu, descriptor);
			}
		}
		m_add_menu->refresh();
		return;
	}

	std::vector<ComponentGroup> groups = editors.get_groups();
	groups.push_back({ .name = std::string(ComponentEditors::K_OTHER_GROUP) });
	const auto group_of = [&editors, &groups](const ComponentDescriptor *descriptor) -> std::string_view {
		const ComponentEditor *editor = editors.find(descriptor->get_name());
		if (editor == nullptr || std::ranges::find(groups, editor->group, &ComponentGroup::name) == groups.end()) {
			return ComponentEditors::K_OTHER_GROUP;
		}
		return editor->group;
	};

	m_add_menu->add_separator();
	for (const ComponentGroup &group : groups) {
		PopupMenu *submenu = nullptr;
		for (const ComponentDescriptor *descriptor : addable) {
			if (group_of(descriptor) != group.name) {
				continue;
			}
			if (submenu == nullptr) {
				submenu = m_add_menu->add_submenu(group.name, m_context.icon(group.icon));
			}
			add_item(submenu, descriptor);
		}
	}
	m_add_menu->refresh();
}

}
