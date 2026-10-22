#include "Aquila/Graphics/Texture/MipChain.h"

#include "Aquila/GFX/GfxContext.h"

namespace Aquila::Graphics::Texture {

namespace {

const std::array<F32, 256> &srgb_to_linear_table() {
	static const std::array<F32, 256> table = [] {
		std::array<F32, 256> out{};
		for (Usize i = 0; i < out.size(); ++i) {
			const F32 c = static_cast<F32>(i) / 255.F;
			out[i] = c <= 0.04045F ? c / 12.92F : std::pow((c + 0.055F) / 1.055F, 2.4F);
		}
		return out;
	}();
	return table;
}

Uint8 unit_to_byte(F32 value) {
	return static_cast<Uint8>(std::lround(std::clamp(value, 0.F, 1.F) * 255.F));
}

std::vector<Uint8> downsample(const std::vector<Uint8> &source, Uint32 width, Uint32 height, MipFilter filter) {
	const Uint32 next_width = std::max(1U, width / 2U);
	const Uint32 next_height = std::max(1U, height / 2U);
	const auto &to_linear = srgb_to_linear_table();
	std::vector<Uint8> target(static_cast<Usize>(next_width) * next_height * 4U);
	for (Uint32 y = 0; y < next_height; ++y) {
		for (Uint32 x = 0; x < next_width; ++x) {
			std::array<const Uint8 *, 4> texels{};
			for (Uint32 i = 0; i < 4; ++i) {
				const Uint32 sx = std::min(width - 1, (x * 2U) + (i % 2U));
				const Uint32 sy = std::min(height - 1, (y * 2U) + (i / 2U));
				texels[i] = &source[((static_cast<Usize>(sy) * width) + sx) * 4U];
			}
			Uint8 *out = &target[((static_cast<Usize>(y) * next_width) + x) * 4U];

			if (filter == MipFilter::AlphaWeighted) {
				std::array<F32, 3> premultiplied{};
				F32 alpha = 0.F;
				for (const Uint8 *texel : texels) {
					const F32 a = static_cast<F32>(texel[3]) / 255.F;
					for (Usize c = 0; c < 3; ++c) {
						premultiplied[c] += static_cast<F32>(texel[c]) * a;
					}
					alpha += a;
				}
				for (Usize c = 0; c < 3; ++c) {
					out[c] = alpha > 0.F ? static_cast<Uint8>(std::lround(premultiplied[c] / alpha)) : 0U;
				}
				out[3] = static_cast<Uint8>(std::lround(alpha * 0.25F * 255.F));
				continue;
			}

			for (Usize c = 0; c < 4; ++c) {
				const bool linearize = filter == MipFilter::BoxSrgb && c < 3;
				F32 sum = 0.F;
				for (const Uint8 *texel : texels) {
					sum += linearize ? to_linear[texel[c]] : static_cast<F32>(texel[c]) / 255.F;
				}
				const F32 average = sum * 0.25F;
				out[c] = linearize ? linear_to_srgb(average) : unit_to_byte(average);
			}
		}
	}
	return target;
}

}

F32 srgb_to_linear(Uint8 value) {
	return srgb_to_linear_table()[value];
}

Uint8 linear_to_srgb(F32 linear) {
	const F32 c = std::clamp(linear, 0.F, 1.F);
	const F32 encoded = c <= 0.0031308F ? c * 12.92F : 1.055F * std::pow(c, 1.F / 2.4F) - 0.055F;
	return static_cast<Uint8>(std::lround(encoded * 255.F));
}

MipChain build_mip_chain(const Uint8 *rgba, Uint32 width, Uint32 height, MipFilter filter, Uint32 max_size) {
	MipChain chain{ .width = width, .height = height, .levels = {} };
	std::vector<Uint8> level(rgba, rgba + (static_cast<Usize>(width) * height * 4U));
	while (max_size > 0 && std::max(chain.width, chain.height) > max_size) {
		level = downsample(level, chain.width, chain.height, filter);
		chain.width = std::max(1U, chain.width / 2U);
		chain.height = std::max(1U, chain.height / 2U);
	}

	chain.levels.push_back(std::move(level));
	Uint32 w = chain.width;
	Uint32 h = chain.height;
	while (w > 1 || h > 1) {
		chain.levels.push_back(downsample(chain.levels.back(), w, h, filter));
		w = std::max(1U, w / 2U);
		h = std::max(1U, h / 2U);
	}
	return chain;
}

Ref<GFX::GfxTexture> upload_mip_chain(GFX::GfxContext &ctx, const MipChain &chain, RHI::TextureFormat format,
									  const RHI::SamplerDesc &sampler, std::string debug_name) {
	RHI::TextureDesc desc{};
	desc.width = chain.width;
	desc.height = chain.height;
	desc.format = format;
	desc.usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst;
	desc.mip_levels = static_cast<Uint32>(chain.levels.size());
	desc.sampler = sampler;
	desc.debug_name = std::move(debug_name);
	Ref<GFX::GfxTexture> texture = ctx.create_texture(desc);
	if (texture) {
		ctx.upload_texture_mips(*texture, chain.levels);
	}
	return texture;
}

}
