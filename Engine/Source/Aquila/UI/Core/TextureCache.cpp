#include "Aquila/UI/Core/TextureCache.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/RHI/Backend/RHITypes.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"

#include "stb/stb_image.h"

#include <lunasvg.h>

namespace Aquila::UI::Core {

namespace {

std::vector<std::vector<Uint8>> build_mip_chain(const Uint8 *pixels, Uint32 width, Uint32 height) {
	std::vector<std::vector<Uint8>> levels;
	levels.emplace_back(pixels, pixels + (static_cast<Usize>(width) * height * 4U));

	Uint32 w = width;
	Uint32 h = height;
	while (w > 1 || h > 1) {
		const Uint32 next_w = std::max(1U, w / 2U);
		const Uint32 next_h = std::max(1U, h / 2U);
		const std::vector<Uint8> &src = levels.back();
		std::vector<Uint8> dst(static_cast<Usize>(next_w) * next_h * 4U);

		for (Uint32 y = 0; y < next_h; ++y) {
			for (Uint32 x = 0; x < next_w; ++x) {
				F32 premultiplied[3] = { 0.F, 0.F, 0.F };
				F32 alpha = 0.F;
				for (Uint32 dy = 0; dy < 2; ++dy) {
					for (Uint32 dx = 0; dx < 2; ++dx) {
						const Uint32 sx = std::min(w - 1, (x * 2U) + dx);
						const Uint32 sy = std::min(h - 1, (y * 2U) + dy);
						const Uint8 *texel = &src[((static_cast<Usize>(sy) * w) + sx) * 4U];
						const F32 a = static_cast<F32>(texel[3]) / 255.F;
						for (int c = 0; c < 3; ++c) {
							premultiplied[c] += static_cast<F32>(texel[c]) * a;
						}
						alpha += a;
					}
				}
				Uint8 *out = &dst[((static_cast<Usize>(y) * next_w) + x) * 4U];
				for (int c = 0; c < 3; ++c) {
					out[c] = alpha > 0.F ? static_cast<Uint8>(std::lround(premultiplied[c] / alpha)) : 0U;
				}
				out[3] = static_cast<Uint8>(std::lround(alpha * 0.25F * 255.F));
			}
		}

		levels.push_back(std::move(dst));
		w = next_w;
		h = next_h;
	}
	return levels;
}

std::unordered_map<const GFX::GfxTexture *, TextureCache *> &vector_owners() {
	static std::unordered_map<const GFX::GfxTexture *, TextureCache *> owners;
	return owners;
}

bool is_svg(const std::string &path) {
	return path.size() >= 4 && path.compare(path.size() - 4, 4, ".svg") == 0;
}

Uint32 size_key(Uint32 width, Uint32 height) {
	return (width << 16U) | (height & 0xFFFFU);
}

}

struct TextureCache::VectorSource {
	std::unique_ptr<lunasvg::Document> document;
	std::string name;
	std::unordered_map<Uint32, Ref<GFX::GfxTexture>> sizes;
};

TextureCache::TextureCache(GFX::GfxContext &ctx, std::string base_path) : m_ctx(ctx), m_base_path(std::move(base_path)) {}

TextureCache::~TextureCache() {
	clear();
}

std::string TextureCache::resolve(const std::string &path) const {
	if (m_base_path.empty() || path.find("://") != std::string::npos || (!path.empty() && path[0] == '/')) {
		return path;
	}
	return m_base_path + "/" + path;
}

GFX::GfxTexture *TextureCache::load(const std::string &path) {
	const std::string resolved = resolve(path);

	auto it = m_cache.find(resolved);
	if (it != m_cache.end()) {
		return it->second.get();
	}

	auto vfile = Platform::Filesystem::VirtualFileSystem::get()->open_file(resolved, AccessMode::Read, OpenMode::Binary);
	if (!vfile || !vfile->is_valid()) {
		AQUILA_LOG_ERROR("TextureCache: cannot open '{}'", resolved);
		return nullptr;
	}
	const Int64 file_size = vfile->size();
	std::vector<Uint8> file_data(static_cast<Usize>(file_size));
	vfile->read(file_data.data(), static_cast<Usize>(file_size));

	return is_svg(resolved) ? load_vector(resolved, file_data) : load_raster(resolved, file_data);
}

GFX::GfxTexture *TextureCache::load_raster(const std::string &resolved, const std::vector<Uint8> &file_data) {
	int width = 0, height = 0, channels = 0;
	stbi_uc *pixels = stbi_load_from_memory(file_data.data(), static_cast<int>(file_data.size()), &width, &height,
											&channels, STBI_rgb_alpha);
	if (pixels == nullptr) {
		AQUILA_LOG_ERROR("TextureCache: failed to load '{}': {}", resolved, stbi_failure_reason());
		return nullptr;
	}

	const std::vector<std::vector<Uint8>> mips =
		build_mip_chain(pixels, static_cast<Uint32>(width), static_cast<Uint32>(height));
	stbi_image_free(pixels);

	RHI::TextureDesc desc{};
	desc.width = static_cast<Uint32>(width);
	desc.height = static_cast<Uint32>(height);
	desc.format = RHI::TextureFormat::RGBA8;
	desc.usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst;
	desc.mip_levels = static_cast<Uint32>(mips.size());
	desc.sampler = RHI::SamplerDesc::ui_image(static_cast<F32>(mips.size()));
	desc.debug_name = resolved;

	Ref<GFX::GfxTexture> tex = m_ctx.create_texture(desc);
	if (!tex) {
		AQUILA_LOG_ERROR("TextureCache: GfxContext::CreateTexture failed for '{}'", resolved);
		return nullptr;
	}

	m_ctx.upload_texture_mips(*tex, mips);

	GFX::GfxTexture *raw = tex.get();
	m_cache.emplace(resolved, std::move(tex));
	return raw;
}

GFX::GfxTexture *TextureCache::load_vector(const std::string &resolved, const std::vector<Uint8> &file_data) {
	auto source = std::make_unique<VectorSource>();
	source->name = resolved;
	source->document =
		lunasvg::Document::loadFromData(reinterpret_cast<const char *>(file_data.data()), file_data.size());
	if (source->document == nullptr) {
		AQUILA_LOG_ERROR("TextureCache: failed to parse SVG '{}'", resolved);
		return nullptr;
	}
	source->document->applyStyleSheet("svg { color: #ffffff; }");

	const Uint32 width = std::max(1U, static_cast<Uint32>(Math::ceil(source->document->width())));
	const Uint32 height = std::max(1U, static_cast<Uint32>(Math::ceil(source->document->height())));
	Ref<GFX::GfxTexture> tex = rasterize(*source, width, height);
	if (!tex) {
		return nullptr;
	}

	GFX::GfxTexture *raw = tex.get();
	m_cache.emplace(resolved, std::move(tex));
	m_vectors.emplace(raw, std::move(source));
	vector_owners()[raw] = this;
	return raw;
}

Ref<GFX::GfxTexture> TextureCache::rasterize(VectorSource &source, Uint32 width, Uint32 height) {
	lunasvg::Bitmap bitmap =
		source.document->renderToBitmap(static_cast<int>(width), static_cast<int>(height), 0x00000000);
	if (bitmap.isNull()) {
		AQUILA_LOG_ERROR("TextureCache: failed to rasterize '{}' at {}x{}", source.name, width, height);
		return nullptr;
	}
	bitmap.convertToRGBA();

	std::vector<Uint8> pixels(static_cast<Usize>(width) * height * 4U);
	for (Uint32 row = 0; row < height; ++row) {
		const Uint8 *src = bitmap.data() + (static_cast<Usize>(row) * static_cast<Usize>(bitmap.stride()));
		std::copy_n(src, static_cast<Usize>(width) * 4U, pixels.begin() + (static_cast<std::ptrdiff_t>(row) * width * 4));
	}

	RHI::TextureDesc desc{};
	desc.width = width;
	desc.height = height;
	desc.format = RHI::TextureFormat::RGBA8;
	desc.usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst;
	desc.sampler = RHI::SamplerDesc{};
	desc.debug_name = source.name;

	Ref<GFX::GfxTexture> tex = m_ctx.create_texture(desc);
	if (!tex) {
		AQUILA_LOG_ERROR("TextureCache: GfxContext::CreateTexture failed for '{}'", source.name);
		return nullptr;
	}
	m_ctx.upload_texture_data(*tex, pixels.data(), pixels.size());
	return tex;
}

GFX::GfxTexture *TextureCache::vector_at_size(GFX::GfxTexture *base, Uint32 width, Uint32 height) {
	auto source = m_vectors.find(base);
	if (source == m_vectors.end()) {
		return base;
	}
	if (width == base->get_width() && height == base->get_height()) {
		return base;
	}

	auto &sizes = source->second->sizes;
	const Uint32 key = size_key(width, height);
	if (auto it = sizes.find(key); it != sizes.end()) {
		return it->second.get();
	}

	Ref<GFX::GfxTexture> tex = rasterize(*source->second, width, height);
	if (!tex) {
		return base;
	}
	GFX::GfxTexture *raw = tex.get();
	sizes.emplace(key, std::move(tex));
	return raw;
}

GFX::GfxTexture *TextureCache::resolve_for_size(GFX::GfxTexture *texture, Vec2 pixel_size) {
	if (texture == nullptr || pixel_size.x < 1.F || pixel_size.y < 1.F) {
		return texture;
	}
	auto owner = vector_owners().find(texture);
	if (owner == vector_owners().end()) {
		return texture;
	}
	constexpr F32 k_max_raster_size = 4096.F;
	const Uint32 width = static_cast<Uint32>(Math::min(Math::round(pixel_size.x), k_max_raster_size));
	const Uint32 height = static_cast<Uint32>(Math::min(Math::round(pixel_size.y), k_max_raster_size));
	return owner->second->vector_at_size(texture, width, height);
}

void TextureCache::forget_vector(GFX::GfxTexture *base) {
	vector_owners().erase(base);
	m_vectors.erase(base);
}

void TextureCache::evict(const std::string &path) {
	auto it = m_cache.find(resolve(path));
	if (it == m_cache.end()) {
		return;
	}
	forget_vector(it->second.get());
	m_cache.erase(it);
}

void TextureCache::clear() {
	for (const auto &[base, source] : m_vectors) {
		vector_owners().erase(base);
	}
	m_vectors.clear();
	m_cache.clear();
}

} // namespace Aquila::UI::Core
