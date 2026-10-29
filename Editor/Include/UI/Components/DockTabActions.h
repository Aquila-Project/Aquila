#ifndef AQUILA_EDITOR_DOCK_TAB_ACTIONS_H
#define AQUILA_EDITOR_DOCK_TAB_ACTIONS_H

#include "Aquila/Foundation/Signal.h"
#include "Aquila/UI/Core/Behaviour.h"

namespace Editor {

class DockTabActions final : public Aquila::UI::Core::Behaviour {
  public:
	void define_behaviour() override;

	Signal<void(Vec2)> on_change_editor;
	Signal<void(Vec2)> on_add_tab;
};

}

#endif
