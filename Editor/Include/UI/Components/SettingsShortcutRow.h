#ifndef AQUILA_EDITOR_SETTINGS_SHORTCUT_ROW_H
#define AQUILA_EDITOR_SETTINGS_SHORTCUT_ROW_H

#include "Aquila/UI/Core/Behaviour.h"

namespace Aquila::UI::Core {
class Button;
class Label;
}

namespace Editor {

class ChordButton;

class SettingsShortcutRow final : public Aquila::UI::Core::Behaviour {
  public:
	void define_behaviour() override;
	void show(const std::string &title, const std::string &shortcut, bool overridden);

	[[nodiscard]] ChordButton *get_chord() const { return m_chord; }
	[[nodiscard]] Aquila::UI::Core::Button *get_reset() const { return m_reset; }

  private:
	Aquila::UI::Core::Label *m_title = nullptr;
	ChordButton *m_chord = nullptr;
	Aquila::UI::Core::Button *m_reset = nullptr;
};

}

#endif
