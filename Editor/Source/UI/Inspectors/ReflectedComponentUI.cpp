#include "UI/Inspectors/ReflectedComponentUI.h"

#include "Aquila/Scene/ComponentRegistry.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"

namespace Editor {

using namespace Aquila;
using namespace Aquila::SceneManagement;

ReflectedComponentUI::ReflectedComponentUI(const ComponentDescriptor &descriptor, GFX::GfxContext *context)
	: m_descriptor(descriptor), m_context(context) {}

bool ReflectedComponentUI::matches(Entity entity) const {
	return m_descriptor.has(entity);
}

void ReflectedComponentUI::build(UI::Core::Collapsible *, UI::Core::PropertyGrid *grid) {
	m_properties = std::make_unique<UI::Core::ReflectedPropertyGrid>(*grid, m_descriptor.get_type(), m_context);
}

bool ReflectedComponentUI::is_removable() const {
	return m_descriptor.is_removable();
}

void ReflectedComponentUI::remove(Entity entity) {
	m_descriptor.remove(entity);
}

void ReflectedComponentUI::show(Entity entity) {
	const ComponentDescriptor &descriptor = m_descriptor;
	m_properties->bind(descriptor.get(entity), [this, &descriptor, entity] {
		on_reflected_edit(entity);
		if (Signal<void()> *changed = descriptor.get_changed_signal(entity)) {
			(*changed)();
		}
		if (on_edited) {
			on_edited(entity);
		}
	});
}

void ReflectedComponentUI::refresh() {
	if (m_properties != nullptr) {
		m_properties->refresh();
	}
}

}
