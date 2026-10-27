#ifndef AQUILA_UI_CORE_WINDOW_BEHAVIOUR_H
#define AQUILA_UI_CORE_WINDOW_BEHAVIOUR_H

#include "Aquila/UI/Core/Behaviour.h"

namespace Aquila::Platform::Events {
class Event;
}

namespace Aquila::UI::Core {

class WindowBehaviour : public Behaviour {
  public:
	explicit WindowBehaviour(std::string layout_path);

	[[nodiscard]] const std::string &get_layout_path() const { return m_layout_path; }

	virtual void update(F32 delta_time) {}
	virtual bool intercept_event(Platform::Events::Event &event) { return false; }

	[[nodiscard]] View *overlay_root() const { return m_overlay_root; }
	void set_overlay_root(View *root) { m_overlay_root = root; }

	void close();
	void set_close_handler(Delegate<void()> handler) { m_close = std::move(handler); }

  private:
	std::string m_layout_path;
	View *m_overlay_root = nullptr;
	Delegate<void()> m_close;
};

}

#endif
