#ifndef AQUILA_EDITOR_TODO_ROW_H
#define AQUILA_EDITOR_TODO_ROW_H

#include "Aquila/Foundation/Signal.h"
#include "Aquila/UI/Core/Behaviour.h"

namespace Aquila::UI::Core {
class Checkbox;
class Label;
}

namespace Editor {

class TodoRow final : public Aquila::UI::Core::Behaviour {
  public:
	void define_behaviour() override;
	void set_done(bool done);

	Signal<void(bool)> on_done_changed;
	Signal<void()> on_remove;

  private:
	Aquila::UI::Core::Checkbox *m_done = nullptr;
	Aquila::UI::Core::Label *m_name = nullptr;
};

}

#endif
