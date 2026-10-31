#ifndef AQUILA_EDITOR_CHORD_BUTTON_H
#define AQUILA_EDITOR_CHORD_BUTTON_H

#include "Aquila/Platform/Events/KeyChord.h"
#include "Aquila/UI/Widgets/Button.h"

namespace Editor {

class ChordButton final : public Aquila::UI::Core::Button {
  public:
	explicit ChordButton(std::string label = {});

	[[nodiscard]] std::string_view get_type_name() const override { return "ChordButton"; }

	void set_label(std::string label);

	Delegate<void(Option<Aquila::Platform::Events::KeyChord>)> on_chord;

	void on_key_press(Aquila::Platform::KeyCode key, int mods = 0) override;
	void on_focus_lost() override;

  private:
	void start_capture();
	void stop_capture();
	void finish(Option<Aquila::Platform::Events::KeyChord> chord);

	std::string m_label;
	bool m_capturing = false;
};

}

#endif
