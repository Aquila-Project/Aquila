#ifndef AQUILA_GRAPHICS_TEXTURE_MIP_CHAIN_H
#define AQUILA_GRAPHICS_TEXTURE_MIP_CHAIN_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/RHI/Backend/RHITypes.h"

namespace Aquila::GFX {
class GfxContext;
class GfxTexture;
}

namespace Aquila::Graphics::Texture {

enum class MipFilter : Uint8 {
	/// Normals, roughness, masks.
	Box,
	/// sRGB color images.
	BoxSrgb,
	/// Transparent texels don't bleed their color in. UI images and thumbnails.
	AlphaWeighted,
};

struct MipChain {
	Uint32 width = 0;
	Uint32 height = 0;
	std::vector<std::vector<Uint8>> levels;
};

/// `max_size` (0 = no limit) drops the levels above it. Any thread.
[[nodiscard]] MipChain build_mip_chain(const Uint8 *rgba, Uint32 width, Uint32 height, MipFilter filter,
									   Uint32 max_size = 0);

/// Null when the texture can't be made.
[[nodiscard]] Ref<GFX::GfxTexture> upload_mip_chain(GFX::GfxContext &ctx, const MipChain &chain,
													RHI::TextureFormat format, const RHI::SamplerDesc &sampler,
													std::string debug_name);

[[nodiscard]] F32 srgb_to_linear(Uint8 value);
[[nodiscard]] Uint8 linear_to_srgb(F32 linear);

}

#endif
