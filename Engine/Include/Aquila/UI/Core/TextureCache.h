#pragma once

#include "Aquila/Foundation/Defines.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxTexture.h"

namespace Aquila::UI::Core {

class TextureCache {
  public:
	explicit TextureCache(GFX::GfxContext &ctx, std::string base_path = {});
	~TextureCache();

	AQUILA_NONCOPYABLE(TextureCache);
	AQUILA_NONMOVEABLE(TextureCache);

	/// Relative paths resolve against the base path. Cached by path, an SVG is parsed once.
	[[nodiscard]] GFX::GfxTexture *load(const std::string &path);

	void evict(const std::string &path);

	void evict_project_assets();
	void clear();

	/// For an SVG, its raster at exactly this pixel size (made on first use, then cached).
	/// Anything else comes back as it is.
	[[nodiscard]] static GFX::GfxTexture *resolve_for_size(GFX::GfxTexture *texture, Vec2 pixel_size);

  private:
	struct VectorSource;

	[[nodiscard]] std::string resolve(const std::string &path) const;
	[[nodiscard]] GFX::GfxTexture *load_raster(const std::string &resolved, const std::vector<Uint8> &file_data);
	[[nodiscard]] GFX::GfxTexture *load_vector(const std::string &resolved, const std::vector<Uint8> &file_data);
	[[nodiscard]] Ref<GFX::GfxTexture> rasterize(VectorSource &source, Uint32 width, Uint32 height);
	[[nodiscard]] GFX::GfxTexture *vector_at_size(GFX::GfxTexture *base, Uint32 width, Uint32 height);
	void forget_vector(GFX::GfxTexture *base);

	GFX::GfxContext &m_ctx;
	std::string m_base_path;

	std::unordered_map<std::string, Ref<GFX::GfxTexture>> m_cache;
	std::unordered_map<const GFX::GfxTexture *, Unique<VectorSource>> m_vectors;
};

} // namespace Aquila::UI::Core
