#include "UI/Inspector/EntityInspector.h"

#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/Platform/Input.h"
#include "Aquila/Scene/ComponentRegistry.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Core/ComponentEditors.h"
#include "Core/EditorContext.h"

namespace Editor {

using Aquila::SceneManagement::ComponentDescriptor;
using Aquila::SceneManagement::Entity;
using Aquila::UI::Core::Inspectable;
using Aquila::UI::Core::InspectorSection;
using Aquila::UI::Core::InspectorTab;
using Aquila::UI::Core::PopupMenu;

EntityInspector::EntityInspector(EditorContext &context) : m_context(context) {
	Aquila::UI::Core::View *overlay = m_context.overlay_root();
	if (overlay == nullptr) {
		return;
	}
	m_add_menu = overlay->add_child(std::make_unique<PopupMenu>());
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
	inspectable.id = uuid;
	inspectable.subtitle = uuid.substr(0, 8);
	inspectable.subtitle_tooltip = uuid;

	const std::vector<ComponentGroup> &groups = editors.get_groups();
	std::vector<InspectorTab> tabs;
	for (const ComponentGroup &group : groups) {
		tabs.push_back({ .title = group.name, .icon = group.icon });
	}
	for (const ComponentDescriptor *descriptor : editors.ordered()) {
		if (descriptor->has(entity)) {
			const auto group =
				std::ranges::find(groups, editors.group_of(descriptor->get_name()), &ComponentGroup::name);
			tabs[static_cast<Usize>(group - groups.begin())].sections.push_back(make_section(entity, *descriptor));
		}
	}
	for (Usize i = 0; i < tabs.size(); ++i) {
		const bool addable = can_add(groups[i].name);
		if (tabs[i].sections.empty() && !addable) {
			continue;
		}
		tabs[i].always_visible = true;
		if (addable) {
			tabs[i].actions.push_back({
				.text = "Add Component",
				.icon = "plus",
				.run =
					[this, group = groups[i].name] {
						open_add_menu(group, Aquila::Platform::Input::get_mouse_position(), false);
					},
			});
		}
		inspectable.tabs.push_back(std::move(tabs[i]));
	}
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
	if (on_component_added) {
		on_component_added(m_context.component_editors.group_of(d.get_name()));
	}
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

bool EntityInspector::can_add(std::string_view group) const {
	return !addable(group).empty();
}

void EntityInspector::open_add_menu(std::string_view group, Vec2 position, bool from_keyboard) {
	if (m_add_menu == nullptr || !can_add(group)) {
		return;
	}
	m_add_group = group;
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

	const std::string needle = Aquila::Foundation::to_lower(query);
	if (needle.empty()) {
		m_add_menu->add_separator();
	}
	for (const ComponentDescriptor *descriptor : addable(m_add_group)) {
		if (!needle.empty() && Aquila::Foundation::to_lower(descriptor->get_name()).find(needle) == std::string::npos) {
			continue;
		}
		m_add_menu->add_item(descriptor->get_name(), {},
							 m_context.icon(m_context.component_editors.icon(descriptor->get_name())),
							 [this, descriptor] { add_component(m_entity, *descriptor); });
	}
	m_add_menu->refresh();
}

std::vector<const ComponentDescriptor *> EntityInspector::addable(std::string_view group) const {
	std::vector<const ComponentDescriptor *> descriptors;
	if (!m_entity.exists()) {
		return descriptors;
	}
	const ComponentEditors &editors = m_context.component_editors;
	for (const ComponentDescriptor *descriptor : editors.ordered()) {
		if (descriptor->can_add() && descriptor->is_removable() && !descriptor->has(m_entity) &&
			editors.group_of(descriptor->get_name()) == group) {
			descriptors.push_back(descriptor);
		}
	}
	return descriptors;
}
}
