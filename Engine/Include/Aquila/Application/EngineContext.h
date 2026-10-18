#ifndef AQUILA_APPLICATION_ENGINE_CONTEXT_H
#define AQUILA_APPLICATION_ENGINE_CONTEXT_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::GFX {
class GfxContext;
class GfxTexture;
} // namespace Aquila::GFX

namespace Aquila::SceneManagement {
class Scene;
}

namespace Aquila::Rendering {
class IRenderWindowHost;
class ObjectPickingSystem;
class RenderPipeline;
class Renderer;
class OverlayRenderer;
} // namespace Aquila::Rendering

namespace Aquila::Application {

class Application;
class Window;

/// Stopped is edit time, nothing simulates. A built game starts playing on its own, the editor on Play.
enum class PlayState : Uint8 { Stopped, Playing, Paused };

/// Everything a module can reach in the engine. Valid from on_attach() to on_detach().
class EngineContext {
  public:
	explicit EngineContext(Application &application) : m_application(application) {}

	[[nodiscard]] Window &get_window() const;
	[[nodiscard]] GFX::GfxContext &get_context() const;
	[[nodiscard]] SceneManagement::Scene &get_scene() const;
	[[nodiscard]] Rendering::RenderPipeline &get_render_pipeline() const;
	[[nodiscard]] Rendering::Renderer &get_renderer() const;
	[[nodiscard]] Rendering::OverlayRenderer &get_overlay_renderer() const;
	[[nodiscard]] Rendering::ObjectPickingSystem &get_object_picking() const;
	[[nodiscard]] GFX::GfxTexture &get_render_output() const;
	[[nodiscard]] Rendering::IRenderWindowHost &get_window_host() const;

	/// Renders the 3D scene at this size instead of the window size, from the next frame on.
	/// The editor passes its viewport size.
	void request_render_resize(Uint32 width, Uint32 height) const;
	void request_close() const;

	/// Only from stopped. Modules get IModule::on_play(), then frames render back to back until pause() or stop().
	/// The scene isn't copied here, the editor does that around it.
	void play() const;
	/// Only while playing. Frames go back to on demand.
	void pause() const;
	/// Only while paused.
	void resume() const;
	/// Modules get IModule::on_stop() in reverse order. The engine doesn't put the scene back, the editor does.
	void stop() const;
	[[nodiscard]] PlayState get_play_state() const;
  private:
	Application &m_application;
};

} // namespace Aquila::Application

#endif
