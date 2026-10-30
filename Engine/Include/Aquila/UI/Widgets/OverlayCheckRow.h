#ifndef AQUILA_UI_WIDGETS_OVERLAY_CHECK_ROW_H
#define AQUILA_UI_WIDGETS_OVERLAY_CHECK_ROW_H

#include "Aquila/UI/Core/Behaviour.h"

namespace Aquila::UI::Core {

class Checkbox;

class OverlayCheckRow final : public Behaviour {
  public:
	void define_behaviour() override;
	[[nodiscard]] Checkbox *get_check() const { return m_check; }

  private:
	Checkbox *m_check = nullptr;
};

}

#endif
