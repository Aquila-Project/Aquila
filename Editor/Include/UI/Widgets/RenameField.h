#ifndef AQUILA_EDITOR_RENAME_FIELD_H
#define AQUILA_EDITOR_RENAME_FIELD_H

#include "Aquila/UI/Widgets/TextInput.h"

namespace Editor {

class RenameField final : public Aquila::UI::Core::TextInput {
  public:
	RenameField();

	void set_on_done(Delegate<void(Option<std::string>)> done) { m_done = std::move(done); }
	void select_stem();

	void on_key_press(Aquila::Platform::KeyCode key, int mods = 0) override;
	void on_focus_lost() override;

  private:
	void finish(Option<std::string> name);

	Delegate<void(Option<std::string>)> m_done;
	bool m_finished = false;
};

}

#endif
