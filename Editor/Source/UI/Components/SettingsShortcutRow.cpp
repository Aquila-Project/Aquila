#include "UI/Components/SettingsShortcutRow.h"

#include "Aquila/UI/Widgets/Label.h"
#include "UI/Widgets/ChordButton.h"

namespace Editor {

using namespace Aquila::UI::Core;

void SettingsShortcutRow::define_behaviour() {
	m_title = get<Label>("title");
	m_chord = get<ChordButton>("chord");
	m_reset = get<Button>("reset");
}

void SettingsShortcutRow::show(const std::string &title, const std::string &shortcut, bool overridden) {
	if (m_title != nullptr) {
		m_title->set_text(title);
	}
	if (m_chord != nullptr) {
		m_chord->set_label(shortcut.empty() ? std::string("Not set") : shortcut);
		m_chord->set_class("settings-chord-empty", shortcut.empty());
	}
	if (m_reset != nullptr) {
		m_reset->set_hidden(!overridden);
	}
}

}
