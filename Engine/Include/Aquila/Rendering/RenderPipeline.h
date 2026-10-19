#pragma once
#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Rendering/AutoExposure.h"
#include "Aquila/Rendering/Renderers/IRenderer.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Rendering/RenderView.h"
#include "Aquila/Rendering/RenderSettings.h"
#include "Aquila/GFX/GfxTexture.h"

namespace Aquila::GFX {
class GfxContext;
class GfxCommandList;
} // namespace Aquila::GFX

namespace Aquila::SceneManagement {
class Scene;
}

namespace Aquila::Rendering {

/// Runs the renderers into one render graph every frame and owns the scene color, display color, depth and picking
/// targets.
class RenderPipeline {
  public:
	RenderPipeline(GFX::GfxContext &ctx, Uint32 width, Uint32 height);
	~RenderPipeline();

	AQUILA_NONCOPYABLE(RenderPipeline);
	AQUILA_NONMOVEABLE(RenderPipeline);

	template <typename T, typename... Args> T &add(Args &&...args) {
		static_assert(std::is_base_of_v<IRenderer, T>, "T must derive from IRenderer");
		auto renderer = std::make_unique<T>(std::forward<Args>(args)...);
		T &ref = *renderer;
		renderer->on_init(m_ctx);
		m_renderers.push_back(std::move(renderer));
		return ref;
	}

	void render(GFX::GfxCommandList &cmd, SceneManagement::Scene &scene, F32 delta_time);
	/// Resizes first when the size changed.
	void render(GFX::GfxCommandList &cmd, SceneManagement::Scene &scene, F32 delta_time, Uint32 width, Uint32 height);
	void resize(Uint32 width, Uint32 height);
	/// Call right before the app sleeps, so auto exposure gets to finish adapting (it may wait for the GPU and
	/// request one more frame).
	void settle();

	/// Render from this view instead of the scene's active camera. clear_primary_view() goes back.
	void set_primary_view(const RenderView &view) { m_primary_view = view; }
	void clear_primary_view() { m_primary_view.reset(); }

	[[nodiscard]] RenderSettings &get_settings() { return m_settings; }
	/// The EV100 the camera exposed the last frame with: the adapted one with auto exposure, before compensation.
	[[nodiscard]] F32 get_exposure_ev100() const { return m_auto_exposure->get_ev100(); }
	[[nodiscard]] const RenderView &get_last_view() const { return m_last_view; }

	/// The display color: tone mapped, sRGB encoded, with the overlays. Recreated on resize.
	[[nodiscard]] GFX::GfxTexture &get_output() const { return *m_display_color; }
	[[nodiscard]] Uint32 get_width() const { return m_width; }
	[[nodiscard]] Uint32 get_height() const { return m_height; }

	struct PassTiming {
		std::string name;
		F32 milliseconds = 0.F;
	};

	/// GPU time per pass from an earlier frame, read without waiting. Empty until the GPU catches up.
	[[nodiscard]] const std::vector<PassTiming> &get_pass_timings() const { return m_pass_timings; }
	[[nodiscard]] F32 get_gpu_frame_milliseconds() const { return m_gpu_frame_ms; }
	[[nodiscard]] bool has_gpu_timings() const { return m_timestamp_pools[0] != nullptr; }

  private:
	void build_frame_context(SceneManagement::Scene &scene, F32 delta_time, const RenderView &primary,
							 FrameContext &out);
	void rebuild_targets();

	[[nodiscard]] RenderView resolve_primary_view(SceneManagement::Scene &scene) const;

	GFX::GfxContext &m_ctx;
	Graphics::RG::RenderGraph m_graph;
	std::vector<Unique<IRenderer>> m_renderers;

	Ref<GFX::GfxTexture> m_scene_color;
	Ref<GFX::GfxTexture> m_display_color;
	Ref<GFX::GfxTexture> m_depth_tex;
	Ref<GFX::GfxTexture> m_object_picking;

	Uint32 m_width = 0;
	Uint32 m_height = 0;

	// Rotates 0..MAX_FRAMES_IN_FLIGHT-1 each Render() call, matching swapchain fence rotation.
	Uint32 m_frame_slot = 0;

	void read_pass_timings(Uint32 slot);

	std::array<Unique<RHI::IRHIQueryPool>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_timestamp_pools;
	std::array<std::vector<std::string>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_timed_passes;
	std::vector<PassTiming> m_pass_timings;
	F32 m_gpu_frame_ms = 0.F;

	Option<RenderView> m_primary_view;
	RenderSettings m_settings;
	Unique<AutoExposure> m_auto_exposure;
	RenderView m_last_view;
};

} // namespace Aquila::Rendering
