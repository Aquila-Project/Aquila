#pragma once
#include "Aquila/Rendering/Systems/RenderingSystemBase.h"
#include "Aquila/Rendering/Environment/HosekWilkieSky.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Graphics/Shader/ReloadablePipeline.h"
#include "Aquila/GFX/GfxBuffer.h"
#include "Aquila/GFX/GfxDescriptorSet.h"

#include <entt.h>

namespace Aquila::Rendering {

/// Draws the active procedural sky light and bakes SceneFrameData's reflection cube from it. The sky is first rendered
/// into one source cube with a full mip chain, the reflection bake only ever reads that cube.
class SkySystem : public RenderingSystemBase {
  public:
	SkySystem() = default;
	~SkySystem() override = default;

	static constexpr Uint32 K_SOURCE_SIZE = 256;
	static constexpr Uint32 K_SOURCE_MIPS = 9;

	void on_init(GFX::GfxContext &ctx) override;
	/// Updates the sky data and, when the sky changed, bakes SceneFrameData's reflection cube, which lit shaders read
	/// before add_passes() runs. Also while the sky itself is hidden.
	void prepare_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;
	void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;

	/// Radiance in nits, before the sky's tint and intensity.
	[[nodiscard]] GFX::GfxTexture &get_source_cube() const { return *m_source_cube; }

  private:
	void add_bake_passes(Graphics::RG::RenderGraph &graph, Uint32 frame_slot);

	Ref<Graphics::Shader::ReloadablePipeline> m_pipeline;
	Ref<Graphics::Shader::ReloadablePipeline> m_cube_pipeline;
	Ref<Graphics::Shader::ReloadablePipeline> m_prefilter_pipeline;
	/// Whether the reflection cube holds the current sky. Reset when a bake shader is reloaded.
	bool m_prefiltered = false;
	bool m_has_sky = false;
	entt::entity m_sky_entity = entt::null;
	Ref<GFX::GfxDescriptorSetLayout> m_sky_layout;
	Ref<GFX::GfxDescriptorSetLayout> m_cube_layout;
	std::array<Ref<GFX::GfxBuffer>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_sky_buffers;
	std::array<Ref<GFX::GfxDescriptorSet>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_sky_sets;
	Ref<GFX::GfxTexture> m_source_cube;
	Ref<GFX::GfxDescriptorSet> m_cube_set;
	GpuSkyData m_cached{};
};

} // namespace Aquila::Rendering
