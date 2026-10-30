#include "UI/Components/HierarchyRow.h"

#include "Aquila/UI/Widgets/Button.h"
#include "UI/Panels/HierarchyTreeView.h"

namespace Editor {

void HierarchyRow::define_behaviour() {
	m_eye = get<Aquila::UI::Core::Button>("eye");
	if (m_eye != nullptr) {
		listen(m_eye->on_click, [this] { on_toggle_visibility(); });
	}
}

void HierarchyRow::set_visible(bool visible, bool hovered, const EntityIcons &icons) {
	if (m_eye == nullptr) {
		return;
	}
	const bool shown = hovered || !visible;
	m_eye->set_icon(shown ? (visible ? icons.eye : icons.eye_off) : nullptr);
	m_eye->set_tooltip(visible ? "Hide" : "Show");
	m_eye->set_class("hierarchy-eye-shown", shown);
}

}
