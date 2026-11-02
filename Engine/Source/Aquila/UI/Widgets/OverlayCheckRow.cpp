#include "Aquila/UI/Widgets/OverlayCheckRow.h"

#include "Aquila/UI/Widgets/Checkbox.h"

namespace Aquila::UI::Core {

void OverlayCheckRow::define_behaviour() {
	m_check = get<Checkbox>("check");
}

}
