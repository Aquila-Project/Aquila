#include "UI/Widgets/RenameField.h"

namespace Editor {

using Aquila::Platform::KeyCode;

RenameField::RenameField() {
	on_submit.connect([this](const std::string &text) { finish(text); });
}

void RenameField::select_stem() {
	const Usize dot = m_state.text.find('.');
	m_state.select_anchor = 0;
	m_state.cursor = dot == 0 || dot == std::string::npos ? m_state.text.size() : dot;
	queue_redraw();
}

void RenameField::on_key_press(KeyCode key, int mods) {
	if (key == KeyCode::Escape) {
		finish(std::nullopt);
		return;
	}
	TextInput::on_key_press(key, mods);
}

void RenameField::on_focus_lost() {
	TextInput::on_focus_lost();
	finish(get_text());
}

void RenameField::finish(Option<std::string> name) {
	if (m_finished) {
		return;
	}
	m_finished = true;
	if (m_done) {
		m_done(std::move(name));
	}
}

}
