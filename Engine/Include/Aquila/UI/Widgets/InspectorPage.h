#ifndef AQUILA_UI_WIDGETS_INSPECTOR_PAGE_H
#define AQUILA_UI_WIDGETS_INSPECTOR_PAGE_H

#include "Aquila/UI/Core/Behaviour.h"

namespace Aquila::UI::Core {

class InspectorPage final : public Behaviour {
  public:
	void define_behaviour() override;
	[[nodiscard]] View &get_sections() const;

  private:
	View *m_sections = nullptr;
};

}

#endif
