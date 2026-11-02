#ifndef AQUILA_EDITOR_HIERARCHY_ROW_H
#define AQUILA_EDITOR_HIERARCHY_ROW_H

#include "Aquila/Foundation/Signal.h"
#include "Aquila/UI/Core/Behaviour.h"

namespace Aquila::UI::Core {
class Button;
}

namespace Editor {

struct EntityIcons;

class HierarchyRow final : public Aquila::UI::Core::Behaviour {
  public:
	void define_behaviour() override;
	void set_visible(bool visible, bool hovered, const EntityIcons &icons);
	[[nodiscard]] Aquila::UI::Core::Button *get_eye() const { return m_eye; }

	Signal<void()> on_toggle_visibility;

  private:
	Aquila::UI::Core::Button *m_eye = nullptr;
};

}

#endif
