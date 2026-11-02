#ifndef AQUILA_EDITOR_TODO_WINDOW_H
#define AQUILA_EDITOR_TODO_WINDOW_H

#include "Core/EditorWindow.h"

namespace Aquila::UI::Core {
class Dropdown;
class TextInput;
}

namespace Editor {

class TodoWindow final : public EditorWindow {
  public:
	explicit TodoWindow(EditorContext &context);

	void define_behaviour() override;
	void update(F32 delta_time) override;

  private:
	struct Task {
		std::string text;
		bool done = false;
		bool removed = false;
	};

	void add_task();
	void refresh();
	void rebuild();

	std::vector<Task> m_tasks;
	bool m_dirty = false;
	Aquila::UI::Core::TextInput *m_input = nullptr;
	Aquila::UI::Core::Dropdown *m_filter = nullptr;
	Aquila::UI::Core::View *m_list = nullptr;
	Aquila::UI::Core::View *m_empty = nullptr;
};

}

#endif
