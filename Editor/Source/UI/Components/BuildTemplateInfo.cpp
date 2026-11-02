#include "UI/Components/BuildTemplateInfo.h"

#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/SelectableTextView.h"

namespace Editor {

void BuildTemplateInfo::define_behaviour() {
	m_state = get<Aquila::UI::Core::Label>("state");
	m_path = get<Aquila::UI::Core::SelectableTextView>("path");
}

void BuildTemplateInfo::show(bool found, const std::string &directory) {
	if (m_state != nullptr) {
		m_state->set_text(found ? "Found" : "Missing: build the PlayerTemplate target");
		m_state->set_class("build-template-found", found);
		m_state->set_class("build-template-missing", !found);
	}
	if (m_path != nullptr) {
		m_path->clear();
		m_path->add_line(directory);
	}
}

}
