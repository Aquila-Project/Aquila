#include "UI/Components/SettingsSection.h"

#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"

namespace Editor {

using namespace Aquila::UI::Core;

void SettingsSection::define_behaviour() {
	m_title = get<Label>("title");
	m_description = get<Label>("description");
	m_grid = get<PropertyGrid>("grid");
	m_reset = get<Button>("reset");
}

void SettingsSection::show(const std::string &title, const std::string &description, bool full,
						   const std::string &reset_text) {
	if (m_title != nullptr) {
		m_title->set_text(title);
		m_title->set_class("settings-page-title", full);
		m_title->set_class("settings-section-title", !full);
	}
	if (m_description != nullptr) {
		m_description->set_text(description);
		m_description->set_hidden(!full || description.empty());
	}
	if (m_reset != nullptr) {
		m_reset->set_text(reset_text);
		m_reset->set_hidden(reset_text.empty());
	}
}

View *SettingsSection::add(std::string_view component, ComponentArgs args) {
	View *added = create(component, root(), std::move(args));
	if (added != nullptr && m_reset != nullptr) {
		root().reorder_child(added, m_reset);
	}
	return added;
}

}
