#ifndef AQUILA_APPLICATION_I_MODULE_H
#define AQUILA_APPLICATION_I_MODULE_H

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Platform::Events {
class Event;
}

namespace Aquila::Application {

class EngineContext;

/// How features plug into the engine. The editor is one, so are scripts.
///
/// @code
/// class StatsModule : public IModule {
///   public:
///     void on_attach(EngineContext &engine) override { m_scene = &engine.get_scene(); }
///     void on_pre_render(F32 delta_time) override { count_entities(*m_scene); }
///
///   private:
///     SceneManagement::Scene *m_scene = nullptr;
/// };
///
/// app.add_module<StatsModule>();
/// @endcode
class IModule {
  public:
	virtual ~IModule() = default;

	AQUILA_NONCOPYABLE(IModule);
	AQUILA_NONMOVEABLE(IModule);

	/// Rendering and the scene exist by now.
	virtual void on_attach(EngineContext &engine) {}
	/// The GPU is idle. Modules detach in reverse order.
	virtual void on_detach() {}

	/// Once per frame, before rendering. No frame means no call: when you need time to pass,
	/// request frames through FrameScheduler.
	virtual void on_pre_render(F32 delta_time) {}
	virtual void on_update(F32 delta_time) {}
	/// Events of the main window, and of secondary windows that forward them.
	virtual void on_event(Platform::Events::Event &event) {}
	virtual void on_resize(Uint32 width, Uint32 height) {}
	/// The 3D render target changed size, see EngineContext::request_render_resize().
	virtual void on_render_resize(Uint32 width, Uint32 height) {}

	/// The game starts (EngineContext::play()), start simulating here. A module attached while the game plays
	/// gets it right after on_attach().
	virtual void on_play() {}
	/// on_pre_render() keeps coming while paused (the editor still redraws), check get_play_state() before
	/// simulating there.
	virtual void on_pause() {}
	virtual void on_resume() {}
	/// Reverse order, like on_detach(). Also runs before on_detach() when the app closes mid game.
	virtual void on_stop() {}

  protected:
	IModule() = default;
};

} // namespace Aquila::Application

#endif
