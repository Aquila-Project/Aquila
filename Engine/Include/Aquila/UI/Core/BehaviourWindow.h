#ifndef AQUILA_UI_CORE_BEHAVIOUR_WINDOW_H
#define AQUILA_UI_CORE_BEHAVIOUR_WINDOW_H

#include "Aquila/UI/Core/CanvasWindow.h"
#include "Aquila/UI/Core/WindowBehaviour.h"

namespace Aquila::UI::Core {

class BehaviourWindow final : public CanvasWindow {
  public:
	BehaviourWindow(LayoutLoader &loader, Unique<WindowBehaviour> window, Uint32 width, Uint32 height);
	~BehaviourWindow() override;

	template <std::derived_from<WindowBehaviour> T> [[nodiscard]] T *get_behaviour() const {
		return dynamic_cast<T *>(m_behaviour);
	}

  private:
	void on_update(F32 delta_time) override;
	bool intercept_event(Platform::Events::Event &event) override;

	WindowBehaviour *m_behaviour = nullptr;
};

}

#endif
