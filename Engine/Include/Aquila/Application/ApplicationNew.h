#ifndef AQUILA_APPLICATION_H
#define AQUILA_APPLICATION_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Foundation/Timer.h"
#include "Aquila/Application/EngineContext.h"
#include "Aquila/Application/IModule.h"
#include "Aquila/Application/Window.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxSwapchain.h"
#include "Aquila/GFX/GfxTexture.h"
#include "Aquila/Platform/InputState.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Rendering/IRenderWindowHost.h"
#include "Aquila/Rendering/RenderPipeline.h"
#include "Aquila/Rendering/Renderers/Renderer.h"
#include "Aquila/Rendering/Renderers/OverlayRenderer.h"

struct ApplicationSpec {
	std::string name = "Aquila";
	Uint32 width = 1920;
	Uint32 height = 1080;
	/// The main window stays hidden until Window::show(), the project launcher uses it.
	bool start_hidden = false;
	/// Off to open the window at width x height instead of maximized.
	bool maximized = true;
	/// Native folders for the engine's own shaders and resources. Empty keeps the source tree, see EnginePaths.
	std::string shaders_dir;
	std::string resources_dir;
	/// Native path to a PNG for the window and taskbar icon. Empty keeps the platform default.
	std::string icon;
	/// Watch shader files and rebuild pipelines when they change (every 0.5 s). Shipped games turn it off.
	bool shader_hot_reload = true;
};

namespace Aquila::Graphics {
class Renderer2D;
}

namespace Aquila::Rendering {
class ObjectPickingSystem;
}

namespace Aquila::Application {

struct RenderWindow {
	Unique<Window> window;
	Ref<GFX::GfxSwapchain> swapchain;

	Ref<GFX::GfxTexture> msaa_color;
	Ref<GFX::GfxRenderPass> render_pass;
	bool needs_resize = false;

	Delegate<void(F32)> on_update;
	Delegate<void(Graphics::Renderer2D &, GFX::GfxCommandList &)> on_render;
	Delegate<void(Platform::Events::Event &)> on_event;
	Delegate<void()> on_close;
	Delegate<Platform::CursorType()> cursor;
	bool forward_events = false;
};

/// Owns the window, GPU context, scene and render pipeline. Everything else plugs in as a module.
///
/// @code
/// Application app({ .name = "Aquila Studio" });
/// app.add_module<Editor::EditorModule>();
/// app.run();
/// @endcode
class Application : public Rendering::IRenderWindowHost {
  public:
	explicit Application(const ApplicationSpec &spec);
	~Application() override;

	AQUILA_NONCOPYABLE(Application);
	AQUILA_NONMOVEABLE(Application);

	/// Before run() the module attaches when the loop starts, after that it attaches right away.
	template <typename T, typename... Args> T &add_module(Args &&...args) {
		static_assert(std::is_base_of_v<IModule, T>, "T must derive from IModule");
		auto module = std::make_unique<T>(std::forward<Args>(args)...);
		T &ref = *module;
		m_modules.push_back(std::move(module));
		if (m_modules_attached && m_attached_modules + 1 == m_modules.size()) {
			attach_module(ref);
			++m_attached_modules;
		}
		return ref;
	}

	/// Blocks until the app closes.
	void run();
	/// The loop stops after the current iteration.
	void close();

	Window &get_window() { return *m_window; }

	Rendering::RenderWindowId create_window(Uint32 width, Uint32 height, const std::string &title,
											Rendering::RenderWindowCallbacks callbacks) override;
	void request_close(Rendering::RenderWindowId window) override;
	[[nodiscard]] Rendering::RenderWindowId get_main_window() const override { return m_window.get(); }
	[[nodiscard]] Vec2 get_window_position(Rendering::RenderWindowId window) const override;
	[[nodiscard]] Vec2 get_window_size(Rendering::RenderWindowId window) const override;
	void set_window_position(Rendering::RenderWindowId window, Vec2 position) override;
	void set_window_min_size(Rendering::RenderWindowId window, Vec2 size) override;

  protected:
	/// Old hooks from when Application was subclassed, nothing uses them. Write an IModule instead.
	virtual void on_init() {}
	virtual void on_shutdown() {}
	virtual void on_pre_render(F32 delta_time) {}
	virtual void on_event(Platform::Events::Event &event) {}
	virtual void on_resize(Uint32 width, Uint32 height) {}
	virtual void on_render_resize(Uint32 width, Uint32 height) {}

	// Request the 3D scene render target to change size, applied before the next frame.
	// Independent of the window/swapchain size so an editor can render at its viewport size.
	void request_render_resize(Uint32 width, Uint32 height);

	GFX::GfxContext &get_context() { return *m_ctx; }
	GFX::GfxTexture &get_render_output() { return m_render_pipeline->get_output(); }
	SceneManagement::Scene &get_scene() { return *m_scene; }
	Rendering::RenderPipeline &get_render_pipeline() { return *m_render_pipeline; }
	Rendering::Renderer &get_renderer() { return *m_renderer; }
	Rendering::OverlayRenderer &get_overlay_renderer() { return *m_overlay_renderer; }
	Rendering::ObjectPickingSystem &get_object_picking() { return *m_object_picking; }

	RenderWindow &create_secondary_window(Uint32 width, Uint32 height, const std::string &title);

  private:
	friend class EngineContext;

	void attach_modules();
	void attach_module(IModule &module);
	void detach_modules();
	void set_play_state(PlayState state);
	void route_window_event(Platform::Events::Event &event);
	void internal_update(F32 delta_time);
	void run_pre_render(F32 delta_time);
	F32 next_frame_delta();
	[[nodiscard]] F64 seconds_until_frame_slot() const;
	[[nodiscard]] Option<F64> idle_timeout() const;
	void poll_shader_hot_reload();
	void internal_on_main_window_event(Platform::Events::Event &event);
	void internal_on_secondary_window_event(RenderWindow &rw, Platform::Events::Event &event);
	void handle_resize();
	void init_rendering(Uint32 width, Uint32 height);

	void render_secondary_windows(bool all);
	void render_one_secondary_window(RenderWindow &rw);
	void ensure_window_targets(RenderWindow &rw, Uint32 width, Uint32 height);

	EngineContext m_engine_context{ *this };
	std::vector<Unique<IModule>> m_modules;
	bool m_modules_attached = false;
	Usize m_attached_modules = 0;
	PlayState m_play_state = PlayState::Stopped;
	Platform::InputState m_game_input;

	ApplicationSpec m_spec;
	Unique<Window> m_window;
	Unique<Foundation::Stopwatch> m_timer;
	bool m_running = true;
	bool m_pending_resize = false;
	bool m_frame_in_progress = false;
	bool m_resumed_from_idle = true;
	F32 m_frame_delta = 0.F;
	F64 m_frame_interval = 0.0;
	Foundation::TimePoint m_last_frame_start{};
	Foundation::TimePoint m_next_hot_reload_check{};

	Uint32 m_render_width = 0;
	Uint32 m_render_height = 0;
	bool m_render_resize_pending = false;
	Uint32 m_next_render_width = 0;
	Uint32 m_next_render_height = 0;

	Unique<GFX::GfxContext> m_ctx;
	Ref<GFX::GfxSwapchain> m_swapchain;
	Unique<SceneManagement::Scene> m_scene;
	Unique<Rendering::RenderPipeline> m_render_pipeline;
	Rendering::Renderer *m_renderer = nullptr;
	Rendering::OverlayRenderer *m_overlay_renderer = nullptr;
	Rendering::ObjectPickingSystem *m_object_picking = nullptr;

	Unique<Graphics::Renderer2D> m_secondary_batcher;
	std::vector<Unique<RenderWindow>> m_secondary_windows;
};

} // namespace Aquila::Application

#endif
