#pragma once
#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/RHI/Backend/IRHIDescriptors.h"
#include "Aquila/GFX/GfxBuffer.h"
#include "Aquila/GFX/GfxTexture.h"

namespace Aquila::GFX {

class GfxContext;
class GfxCommandList;

class GfxDescriptorSetLayout {
  public:
	~GfxDescriptorSetLayout() = default;
	AQUILA_NONCOPYABLE(GfxDescriptorSetLayout);

	[[nodiscard]] Uint32 get_binding_count() const;
	[[nodiscard]] RHI::IRHIDescriptorSetLayout &get_rhi() { return *m_layout; }

  private:
	friend class GfxContext;
	explicit GfxDescriptorSetLayout(Unique<RHI::IRHIDescriptorSetLayout> layout);
	Unique<RHI::IRHIDescriptorSetLayout> m_layout;
};

/// Nothing reaches the GPU until flush().
///
/// @code
/// set->set_buffer(0, *uniforms).set_texture(1, *albedo).flush();
/// @endcode
class GfxDescriptorSet {
  public:
	~GfxDescriptorSet() = default;
	AQUILA_NONCOPYABLE(GfxDescriptorSet);

	GfxDescriptorSet &set_buffer(Uint32 binding, GfxBuffer &buffer, Uint64 offset = 0, Uint64 range = 0);
	GfxDescriptorSet &set_texture(Uint32 binding, GfxTexture &texture);
	void flush();

	[[nodiscard]] RHI::IRHIDescriptorSet &get_rhi() { return *m_set; }

  private:
	friend class GfxContext;
	explicit GfxDescriptorSet(Unique<RHI::IRHIDescriptorSet> set);
	Unique<RHI::IRHIDescriptorSet> m_set;
};

} // namespace Aquila::GFX
