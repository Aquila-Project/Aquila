#ifndef AQUILA_UI_CORE_BEHAVIOUR_WINDOW_H
#define AQUILA_UI_CORE_BEHAVIOUR_WINDOW_H

#include "Aquila/UI/Core/CanvasWindow.h"
#include "Aquila/UI/Core/WindowBehaviour.h"

namespace Aquila::UI::Core {

class BehaviourWindow final : public CanvasWindow {
  public:
	/// Makes the window's behaviour, again on every rebuild(), so wire the behaviour's delegates in it.
	using Factory = Delegate<Unique<WindowBehaviour>()>;

	/// A window built this way can't rebuild().
	BehaviourWindow(LayoutLoader &loader, Unique<WindowBehaviour> window, Uint32 width, Uint32 height);
	BehaviourWindow(LayoutLoader &loader, Factory factory, Uint32 width, Uint32 height);
	~BehaviourWindow() override;

	/// Builds the window again from its markup with a fresh behaviour from the factory, which takes over the old one's
	/// state (WindowBehaviour::take_state). The loader's cached markup is used, see LayoutLoader::forget_documents().
	/// Never call it from the window's own events or update: the old behaviour is destroyed.
	/// @return false without a factory or when the markup doesn't build (the window stays empty until it does)
	bool rebuild();
	/// Sees the window's events before its behaviour, true swallows the event. For app-wide keys.
	void set_event_hook(Delegate<bool(Platform::Events::Event &)> hook) { m_event_hook = std::move(hook); }

	template <std::derived_from<WindowBehaviour> T> [[nodiscard]] T *get_behaviour() const {
		return dynamic_cast<T *>(m_behaviour);
	}

  private:
	void build(Unique<WindowBehaviour> window);
	void on_update(F32 delta_time) override;
	bool intercept_event(Platform::Events::Event &event) override;

	LayoutLoader &m_loader;
	Factory m_factory;
	Delegate<bool(Platform::Events::Event &)> m_event_hook;
	WindowBehaviour *m_behaviour = nullptr;
};

}

#endif
