#ifndef AQUILA_IRHI_DESCRIPTORS_H
#define AQUILA_IRHI_DESCRIPTORS_H

#include "Aquila/RHI/Backend/RHITypes.h"

namespace Aquila::RHI {

// Minimal abstract interface — descriptor systems are heavily API-specific.
// Vulkan provides its full concrete implementation via VulkanDescriptors.h.
// This interface exists as an extension point for future backends.
class IRHIDescriptorSetLayout {
  public:
	virtual ~IRHIDescriptorSetLayout() = default;

	IRHIDescriptorSetLayout(const IRHIDescriptorSetLayout &) = delete;
	IRHIDescriptorSetLayout &operator=(const IRHIDescriptorSetLayout &) = delete;

	[[nodiscard]] virtual Uint32 get_binding_count() const = 0;

  protected:
	IRHIDescriptorSetLayout() = default;
};

class IRHIBuffer;
class IRHITexture;

class IRHIDescriptorSet {
  public:
	virtual ~IRHIDescriptorSet() = default;

	IRHIDescriptorSet(const IRHIDescriptorSet &) = delete;
	IRHIDescriptorSet &operator=(const IRHIDescriptorSet &) = delete;

	/// Writes are collected and applied on flush(). range 0 means the whole buffer.
	virtual void set_buffer(Uint32 binding, IRHIBuffer &buffer, Uint64 offset = 0, Uint64 range = 0) = 0;
	virtual void set_texture(Uint32 binding, IRHITexture &texture) = 0;
	virtual void flush() = 0;

  protected:
	IRHIDescriptorSet() = default;
};

} // namespace Aquila::RHI
#endif
