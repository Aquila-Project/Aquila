#ifndef AQUILA_RENDERING_AUTO_EXPOSURE_H
#define AQUILA_RENDERING_AUTO_EXPOSURE_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/Photometry.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/GFX/GfxBuffer.h"
#include "Aquila/GFX/GfxDescriptorSet.h"
#include "Aquila/Graphics/Shader/ReloadablePipeline.h"
#include "Aquila/Rendering/RenderView.h"

namespace Aquila::GFX {
class GfxContext;
}

namespace Aquila::Graphics::RG {
class RenderGraph;
}

namespace Aquila::Rendering {

struct FrameContext;

/// Eye adaptation. Every lit frame a compute pass bins the scene's luminance into a histogram, the CPU reads it back
/// when that frame slot comes around again and eases the exposure toward what a light meter would pick (the brighter
/// half of the image, minus the hottest highlights), faster toward bright than toward dark. It asks for frames only
/// while it's still adapting. Owned by RenderPipeline. Main thread only.
class AutoExposure {
  public:
	explicit AutoExposure(GFX::GfxContext &ctx);

	AQUILA_NONCOPYABLE(AutoExposure);
	AQUILA_NONMOVEABLE(AutoExposure);

	/// Reads what `frame_slot` measured last time and returns this frame's exposure (pixel value per nit).
	/// With `measure` false (debug views, no camera) this frame isn't measured and the exposure stays put.
	F32 begin_frame(Uint32 frame_slot, const RenderView &view, bool measure, F32 delta_time);
	/// The histogram pass. Call once ctx.h_scene_color is final.
	void add_pass(Graphics::RG::RenderGraph &graph, FrameContext &ctx);
	/// Call right before the app sleeps. The last frames' histograms would never be read otherwise, so it waits for
	/// the GPU, reads the newest one and asks for another frame if the exposure still has to move.
	void settle();

	/// What the camera adapted to, before the compensation.
	[[nodiscard]] F32 get_ev100() const { return m_ev100; }

  private:
	void read_histogram(Uint32 slot);
	[[nodiscard]] bool is_settled() const;

	GFX::GfxContext &m_ctx;
	Ref<GFX::GfxDescriptorSetLayout> m_layout;
	Ref<Graphics::Shader::ReloadablePipeline> m_pipeline;
	std::array<Ref<GFX::GfxBuffer>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_histograms;
	std::array<Ref<GFX::GfxDescriptorSet>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_sets;
	std::array<bool, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_pending{};
	std::array<Uint64, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_measured_at{};

	Uint64 m_frame = 0;
	Uint32 m_slot = 0;
	F32 m_exposure = 1.F;
	F32 m_ev100 = Foundation::Photometry::DAYLIGHT_EV100;
	F32 m_target_ev100 = Foundation::Photometry::DAYLIGHT_EV100;
	bool m_has_target = false;
	bool m_active = false;
	bool m_measure = false;
};

}

#endif
