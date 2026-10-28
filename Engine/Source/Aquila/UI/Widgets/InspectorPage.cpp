#include "Aquila/UI/Widgets/InspectorPage.h"

namespace Aquila::UI::Core {

void InspectorPage::define_behaviour() {
	m_sections = get<View>("sections");
}

View &InspectorPage::get_sections() const {
	return m_sections != nullptr ? *m_sections : root();
}

}
