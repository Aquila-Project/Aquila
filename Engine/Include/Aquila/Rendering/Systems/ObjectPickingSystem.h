#pragma once
#include "Aquila/Rendering/Systems/RenderingSystemBase.h"
#include "Aquila/Graphics/Shader/ReloadablePipeline.h"
#include "Aquila/Foundation/Signal.h"
#include "Aquila/Scene/Entity.h"

namespace Aquila::Rendering {

class ObjectPickingSystem : public RenderingSystemBase {
  public:
	ObjectPickingSystem() = default;
	~ObjectPickingSystem() override = default;

	void on_init(GFX::GfxContext &ctx) override;
	void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;

	/// Pixel in the render target. The answer comes a couple of frames later through on_picked,
	/// with a null entity when nothing was hit.
	void request_pick(Uint32 x, Uint32 y);

	[[nodiscard]] bool has_pending_pick() const { return m_pending_request.has_value(); }

	Signal<void(SceneManagement::Entity)> on_picked;

  private:
	struct PickRequest {
		Uint32 x = 0;
		Uint32 y = 0;
	};

	void resolve_readback(Uint32 frame_slot, SceneManagement::Scene &scene);

	Ref<Graphics::Shader::ReloadablePipeline> m_pipeline;
	std::array<Ref<GFX::GfxBuffer>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_readback_buffers;
	std::array<bool, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_readback_in_flight{};
	Option<PickRequest> m_pending_request;
};

} // namespace Aquila::Rendering
