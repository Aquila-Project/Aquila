#pragma once

#include "Aquila/Scene/Entity.h"
#include "Aquila/UI/Widgets/Inspectable.h"

namespace Aquila::SceneManagement {
class ComponentDescriptor;
}

namespace Editor {

class EditorContext;

/// Components without one still get a reflected section, after the ones that have one.
struct ComponentEditor {
	/// ComponentDescriptor name, like "Mesh".
	std::string component;
	std::string icon = "box";
	Int32 order = 0;
	/// ComponentGroup name, empty or unknown means Other.
	std::string group{};
	/// The entity takes the icon of its highest ranked component, 0 never does.
	Int32 icon_rank = 0;
	/// Replaces ComponentDescriptor::add() (Mesh also adds a default material).
	Delegate<void(Aquila::SceneManagement::Entity, EditorContext &)> add{};
	Delegate<Unique<Aquila::UI::Core::IInspectorContent>(Aquila::SceneManagement::Entity, EditorContext &)> content{};
};

/// One inspector tab and one Add Component submenu.
struct ComponentGroup {
	std::string name;
	std::string icon = "box";
};

/// Inspector, hierarchy icons and Add Component all read this.
class ComponentEditors {
  public:
	ComponentEditors();

	/// Same component replaces the old one.
	void add(ComponentEditor editor);
	/// Groups keep the order they were added in, Other stays last.
	void add_group(ComponentGroup group);

	[[nodiscard]] const ComponentEditor *find(std::string_view component) const;
	[[nodiscard]] std::string_view icon(std::string_view component) const;
	/// Other when the component has no group or one nobody added.
	[[nodiscard]] std::string_view group_of(std::string_view component) const;
	[[nodiscard]] std::string_view entity_icon(Aquila::SceneManagement::Entity entity) const;
	/// Every registered descriptor, in inspector order.
	[[nodiscard]] std::vector<const Aquila::SceneManagement::ComponentDescriptor *> ordered() const;
	[[nodiscard]] const std::vector<ComponentGroup> &get_groups() const { return m_groups; }

	static constexpr std::string_view K_EMPTY_ENTITY_ICON = "circle-dashed";
	static constexpr std::string_view K_FALLBACK_ICON = "box";
	static constexpr std::string_view K_OTHER_GROUP = "Other";

  private:
	std::vector<ComponentEditor> m_editors;
	std::vector<ComponentGroup> m_groups;
};

void add_builtin_component_editors(ComponentEditors &editors);

}
