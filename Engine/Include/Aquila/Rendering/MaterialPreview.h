#ifndef AQUILA_RENDERING_MATERIAL_PREVIEW_H
#define AQUILA_RENDERING_MATERIAL_PREVIEW_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/GFX/GfxTexture.h"

namespace Aquila::GFX {
class GfxBuffer;
class GfxContext;
class GfxDescriptorSet;
class GfxDescriptorSetLayout;
class GfxMesh;
class GfxPipeline;
class GfxRenderPass;
}

namespace Aquila::Graphics {
class Material;
}

namespace Aquila::Rendering {

/// Draws one material on a sphere into a small texture, for thumbnails. Studio light (a key light and a sky
/// gradient), no shadows, AgX and sRGB like the viewport. Main thread only and never inside a frame: every render()
/// waits for the GPU to go idle and submits its own commands, so keep it to a few per frame.
/// @note Needs SceneFrameData, its layout is set 0 of every material.
class MaterialPreview {
  public:
	explicit MaterialPreview(GFX::GfxContext &ctx, Uint32 size = 128);
	~MaterialPreview();

	AQUILA_NONCOPYABLE(MaterialPreview)
	AQUILA_NONMOVEABLE(MaterialPreview);

	/// An RGBA8 texture, transparent around the sphere. Transparent materials get a dark backdrop so the see-through
	/// part shows. Pass the previous result as `target` to draw over it instead of making a new one, so whoever shows
	/// it keeps a valid pointer. Null for fullscreen materials or when the preview couldn't set itself up.
	[[nodiscard]] Ref<GFX::GfxTexture> render(Graphics::Material &material, Ref<GFX::GfxTexture> target = nullptr);

  private:
	void create_resources();
	[[nodiscard]] Ref<GFX::GfxBuffer> make_buffer(Uint64 size, bool uniform, const void *data, const char *name);

	GFX::GfxContext &m_ctx;
	Uint32 m_size = 128;
	bool m_created = false;
	Ref<GFX::GfxMesh> m_sphere;
	Ref<GFX::GfxTexture> m_color;
	Ref<GFX::GfxTexture> m_depth;
	Ref<GFX::GfxRenderPass> m_scene_pass;
	Ref<GFX::GfxRenderPass> m_backdrop_pass;
	std::vector<Ref<GFX::GfxBuffer>> m_buffers;
	Ref<GFX::GfxDescriptorSet> m_scene_set;
	Ref<GFX::GfxDescriptorSetLayout> m_tone_map_layout;
	Ref<GFX::GfxDescriptorSet> m_tone_map_set;
	Ref<GFX::GfxPipeline> m_tone_map_pipeline;
};

}

#endif
