#include "Aquila/GFX/GfxTexture.h"

namespace Aquila::GFX {

GfxTexture::GfxTexture(Unique<RHI::IRHITexture> texture) : m_texture(std::move(texture)) {
	static std::atomic<Uint64> s_next_id{ 1 };
	m_id = s_next_id.fetch_add(1, std::memory_order_relaxed);
}

void GfxTexture::destroy_immediate() {
	m_texture->destroy_immediate();
}

bool GfxTexture::is_ready() const {
	return m_texture->is_ready();
}

Uint32 GfxTexture::get_width() const {
	return m_texture->get_width();
}
Uint32 GfxTexture::get_height() const {
	return m_texture->get_height();
}
Uint32 GfxTexture::get_mip_levels() const {
	return m_texture->get_mip_levels();
}
Uint32 GfxTexture::get_array_layers() const {
	return m_texture->get_array_layers();
}
RHI::TextureFormat GfxTexture::get_format() const {
	return m_texture->get_format();
}
const RHI::TextureDesc &GfxTexture::get_desc() const {
	return m_texture->get_desc();
}

} // namespace Aquila::GFX
