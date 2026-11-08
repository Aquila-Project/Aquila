#ifndef AQUILA_RENDERING_ENVIRONMENT_IMAGE_H
#define AQUILA_RENDERING_ENVIRONMENT_IMAGE_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Graphics/Texture/MipChain.h"
#include "Aquila/Scene/Components/SkyLightComponent.h"

namespace Aquila::Rendering {

/// An equirectangular sky image ready to upload, plus its ambient light. Pixels map to directions the way
/// Equirect.slang's EquirectUV() does: +Z in the middle of the image, +X a quarter to the right of it, +Y the top row.
struct EnvironmentImage {
	/// Widest level kept, bigger images lose their top levels. 4096 x 2048 is about 85 MB on the GPU with its mips.
	static constexpr Uint32 K_MAX_WIDTH = 4096;

	/// RGBA16F, full mip chain. Each level is box filtered from the float level above it, so a sun too bright for a
	/// half float (65504) is clamped in the fine levels but keeps its energy in the coarse ones.
	Graphics::Texture::MipChain mips;
	/// Same convention as HosekWilkieSky::bake_sh(), in the image's own units (the sky's image intensity scales it).
	SceneManagement::Components::SHCoefficients irradiance;

	[[nodiscard]] bool is_valid() const { return !mips.levels.empty(); }

	/// `rgba` is RGBA32F, rows top to bottom. Any thread, but slow for big images: run it on a job.
	[[nodiscard]] static EnvironmentImage build(const F32 *rgba, Uint32 width, Uint32 height);
};

/// SH of an RGBA32F equirectangular image, every pixel weighted by the solid angle it covers. Any thread.
[[nodiscard]] SceneManagement::Components::SHCoefficients bake_equirect_sh(const F32 *rgba, Uint32 width,
																		   Uint32 height);

}

#endif
