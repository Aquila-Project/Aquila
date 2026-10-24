#pragma once

#include "Aquila/Scene/Entity.h"
#include "Aquila/UI/Widgets/Inspectable.h"

namespace Aquila::SceneManagement {
class ComponentDescriptor;
}

namespace Aquila::UI::Core {
class PopupMenu;
class TextInput;
} // namespace Aquila::UI::Core

namespace Editor {

class EditorContext;

/// Make it during EditorWindow::build(): its Add Component menu goes under the overlay root and has to die with
/// the window.
class EntityInspector {
  public:
	explicit EntityInspector(EditorContext &context);
	~EntityInspector();

	AQUILA_NONCOPYABLE(EntityInspector);
	AQUILA_NONMOVEABLE(EntityInspector);

	[[nodiscard]] Aquila::UI::Core::Inspectable inspect(Aquila::SceneManagement::Entity entity);

	/// `position` is in canvas pixels. `from_keyboard` keeps the opening key out of the search field.
	void open_add_menu(Vec2 position, bool from_keyboard);

  private:
	[[nodiscard]] Aquila::UI::Core::InspectorSection
	make_section(Aquila::SceneManagement::Entity entity, const Aquila::SceneManagement::ComponentDescriptor &d);
	void add_component(Aquila::SceneManagement::Entity entity, const Aquila::SceneManagement::ComponentDescriptor &d);
	void remove_component(Aquila::SceneManagement::Entity entity,
						  const Aquila::SceneManagement::ComponentDescriptor &d);
	void populate_add_menu(const std::string &query);

	EditorContext &m_context;
	Aquila::UI::Core::PopupMenu *m_add_menu = nullptr;
	Aquila::UI::Core::TextInput *m_add_search = nullptr;
	Aquila::SceneManagement::Entity m_entity;
};

}
