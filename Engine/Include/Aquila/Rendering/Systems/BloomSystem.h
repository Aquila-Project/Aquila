#ifndef AQUILA_RENDERING_BLOOM_SYSTEM_H
#define AQUILA_RENDERING_BLOOM_SYSTEM_H

#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/GFX/GfxDescriptorSet.h"
#include "Aquila/GFX/GfxTexture.h"
#include "Aquila/Graphics/Shader/ReloadablePipeline.h"
#include "Aquila/Rendering/Systems/RenderingSystemBase.h"

namespace Aquila::Rendering {

/// Glow around emissive surfaces, the Call of Duty: Advanced Warfare way. Draws what Lit materials emit
/// (Surface::emissive, MaterialFactory::get_emissive_pipeline) depth tested against the scene, runs a chain of 13-tap
/// downsamples over it, adds tent upsamples back up the chain and adds the result to the scene color, scaled by the
/// camera's bloom_intensity. Writes FrameContext::h_bloom_final, which ToneMapSystem reads. Add it after everything
/// that draws HDR (fullscreen effects included) and before ToneMapSystem. Lit view only, nothing runs when the camera
/// has bloom off or nothing glows. Materials whose emission parameters say they're dark
/// (Material::get_average_emission) aren't drawn, shaders without such parameters always are.
class BloomSystem : public RenderingSystemBase {
  public:
	/// The first mip is half the render size, every next one half again.
	static constexpr Uint32 k_mip_count = 6;

	BloomSystem() = default;
	~BloomSystem() override = default;

	void on_init(GFX::GfxContext &ctx) override;
	void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;

  private:
	[[nodiscard]] bool pipelines_ready() const;
	void ensure_targets(Uint32 width, Uint32 height);

	Ref<GFX::GfxDescriptorSetLayout> m_layout;
	Ref<GFX::GfxDescriptorSetLayout> m_final_layout;
	std::array<std::array<Ref<GFX::GfxDescriptorSet>, k_mip_count>, SharedConstants::MAX_FRAMES_IN_FLIGHT>
		m_downsample_sets;
	std::array<std::array<Ref<GFX::GfxDescriptorSet>, k_mip_count - 1>, SharedConstants::MAX_FRAMES_IN_FLIGHT>
		m_upsample_sets;
	std::array<Ref<GFX::GfxDescriptorSet>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_final_sets;
	Ref<Graphics::Shader::ReloadablePipeline> m_downsample_pipeline;
	Ref<Graphics::Shader::ReloadablePipeline> m_upsample_pipeline;
	Ref<Graphics::Shader::ReloadablePipeline> m_final_pipeline;

	Ref<GFX::GfxTexture> m_emissive;
	std::array<Ref<GFX::GfxTexture>, k_mip_count> m_mips;
	Ref<GFX::GfxTexture> m_final;
	Uint32 m_width = 0;
	Uint32 m_height = 0;
};

} // namespace Aquila::Rendering

#endif
