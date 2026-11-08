#pragma once
#include "Aquila/Rendering/Systems/RenderingSystemBase.h"
#include "Aquila/Rendering/Environment/HosekWilkieSky.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Graphics/Shader/ReloadablePipeline.h"
#include "Aquila/GFX/GfxBuffer.h"
#include "Aquila/GFX/GfxDescriptorSet.h"

#include <entt.h>

namespace Aquila::Rendering {

/// Draws the active sky light (procedural or an equirectangular image) and bakes SceneFrameData's reflection cube
/// from it. Both kinds are first rendered into one source cube with a full mip chain, the reflection bake only ever
/// reads that cube. Images load on the JobSystem; until one is ready there is no sky and no reflection.
class SkySystem : public RenderingSystemBase {
  public:
	SkySystem();
	~SkySystem() override;

	static constexpr Uint32 K_SOURCE_SIZE = 256;
	static constexpr Uint32 K_SOURCE_MIPS = 9;

	void on_init(GFX::GfxContext &ctx) override;
	/// Updates the sky data and, when the sky changed, bakes SceneFrameData's reflection cube, which lit shaders read
	/// before add_passes() runs. Also while the sky itself is hidden.
	void prepare_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;
	void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;

	/// Radiance before the sky's tint and intensity: nits for the procedural sky, image values for an image.
	[[nodiscard]] GFX::GfxTexture &get_source_cube() const { return *m_source_cube; }
	/// Whether a sky image is being decoded. For tests and loading screens.
	[[nodiscard]] bool is_loading_image() const { return !m_image_path.empty() && !m_image.texture && !m_image_failed; }

  private:
	struct ImageInbox;
	struct LoadedImage {
		Ref<GFX::GfxTexture> texture;
		SceneManagement::Components::SHCoefficients irradiance;
	};

	/// Starts decoding `path` on a job, unless it is the image already loaded or loading.
	void request_image(const std::string &path);
	/// Uploads a finished decode. Main thread.
	void receive_image();
	/// Points `frame_slot`'s sky set at the current image. Only touches the set of the slot being recorded, the
	/// other slot may still be on the GPU.
	void bind_image(Uint32 frame_slot);
	void add_bake_passes(Graphics::RG::RenderGraph &graph, Uint32 frame_slot);

	Ref<Graphics::Shader::ReloadablePipeline> m_pipeline;
	Ref<Graphics::Shader::ReloadablePipeline> m_image_pipeline;
	Ref<Graphics::Shader::ReloadablePipeline> m_cube_pipeline;
	Ref<Graphics::Shader::ReloadablePipeline> m_prefilter_pipeline;
	/// Whether the reflection cube holds the current sky. Reset when a bake shader is reloaded.
	bool m_prefiltered = false;
	bool m_has_sky = false;
	bool m_image_sky = false;
	entt::entity m_sky_entity = entt::null;
	Ref<GFX::GfxDescriptorSetLayout> m_sky_layout;
	Ref<GFX::GfxDescriptorSetLayout> m_cube_layout;
	std::array<Ref<GFX::GfxBuffer>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_sky_buffers;
	std::array<Ref<GFX::GfxDescriptorSet>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_sky_sets;
	std::array<Uint64, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_bound_images{};
	Ref<GFX::GfxTexture> m_source_cube;
	Ref<GFX::GfxDescriptorSet> m_cube_set;
	Ref<GFX::GfxTexture> m_placeholder;
	GpuSkyData m_cached{};

	std::string m_image_path;
	Uint64 m_image_request = 0;
	bool m_image_failed = false;
	LoadedImage m_image;
	Ref<ImageInbox> m_inbox;
};

} // namespace Aquila::Rendering
