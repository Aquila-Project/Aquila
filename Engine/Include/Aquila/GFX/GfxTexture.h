#pragma once
#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/RHI/Backend/IRHITexture.h"

namespace Aquila::GFX {

class GfxContext;

class GfxTexture {
  public:
	~GfxTexture() = default;
	AQUILA_NONCOPYABLE(GfxTexture);

	void destroy_immediate();
	[[nodiscard]] bool is_ready() const;

	[[nodiscard]] Uint32 get_width() const;
	[[nodiscard]] Uint32 get_height() const;
	[[nodiscard]] Uint32 get_mip_levels() const;
	[[nodiscard]] Uint32 get_array_layers() const;
	[[nodiscard]] RHI::TextureFormat get_format() const;
	[[nodiscard]] const RHI::TextureDesc &get_desc() const;
	[[nodiscard]] RHI::IRHITexture &get_rhi() { return *m_texture; }
	/// Unique for the whole run, unlike the address: caches keyed by GfxTexture* compare it to notice a new texture
	/// that landed where a dead one was.
	[[nodiscard]] Uint64 get_id() const { return m_id; }

  private:
	friend class GfxContext;
	explicit GfxTexture(Unique<RHI::IRHITexture> texture);
	Unique<RHI::IRHITexture> m_texture;
	Uint64 m_id = 0;
};

} // namespace Aquila::GFX
