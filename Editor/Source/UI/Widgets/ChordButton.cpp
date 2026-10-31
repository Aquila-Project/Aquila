#include "UI/Widgets/ChordButton.h"

namespace Editor {

using Aquila::Platform::Events::KeyChord;
namespace Platform = Aquila::Platform;

ChordButton::ChordButton(std::string label) : Button(label), m_label(std::move(label)) {
	on_click.connect([this] { start_capture(); });
}

void ChordButton::set_label(std::string label) {
	m_label = std::move(label);
	if (!m_capturing) {
		set_text(m_label);
	}
}

void ChordButton::on_key_press(Platform::KeyCode key, int mods) {
	if (!m_capturing) {
		return;
	}
	if (key == Platform::KeyCode::Escape) {
		stop_capture();
		return;
	}
	if (key == Platform::KeyCode::Backspace || key == Platform::KeyCode::Delete) {
		finish(std::nullopt);
		return;
	}
	const KeyChord chord{ .key = key, .mods = mods & Platform::Events::MODIFIER_MASK };
	if (!chord.is_valid() || chord.is_modifier_only()) {
		return;
	}
	finish(chord);
}

void ChordButton::on_focus_lost() {
	Button::on_focus_lost();
	stop_capture();
}

void ChordButton::start_capture() {
	m_capturing = true;
	set_text("Press a shortcut...");
	add_class("settings-chord-capturing");
	request_focus();
}

void ChordButton::stop_capture() {
	if (!m_capturing) {
		return;
	}
	m_capturing = false;
	set_text(m_label);
	remove_class("settings-chord-capturing");
}

void ChordButton::finish(Option<KeyChord> chord) {
	stop_capture();
	Delegate<void(Option<KeyChord>)> callback = on_chord;
	if (callback) {
		callback(chord);
	}
}

}
