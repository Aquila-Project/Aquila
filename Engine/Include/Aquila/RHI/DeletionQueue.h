#pragma once
#include "GraphicsPCH.h"
#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/SharedConstants.h"

namespace Aquila::RHI {

class VulkanDevice;

namespace Deletion {
struct VmaImageDeletion {
	VkImage image;
	VmaAllocation allocation;
};
struct VmaBufferDeletion {
	VkBuffer buffer;
	VmaAllocation allocation;
};

using ResourceVariant =
	std::variant<VkPipeline, VkShaderModule, VkPipelineLayout, VkPipelineCache, VkImageView, VkDeviceMemory, VkSampler,
				 VkDescriptorSetLayout, VkRenderPass, VkFramebuffer, VkSemaphore, VmaImageDeletion, VmaBufferDeletion>;
} // namespace Deletion

/// Vulkan objects can't be destroyed while a frame in flight still uses them.
/// queue_deletion() puts them in the current frame slot's bucket, and the bucket is flushed
/// when that slot comes back around, after its fence. The main swapchain drives the slots.
class DeletionQueue {
  public:
	explicit DeletionQueue(VulkanDevice &device);
	~DeletionQueue();

	AQUILA_NONCOPYABLE(DeletionQueue);

	void queue_deletion(const Deletion::ResourceVariant &resource);

	/// The GPU must be done with it.
	void destroy_now(const Deletion::ResourceVariant &resource);

	void set_current_slot(Uint32 slot);

	void flush(Uint32 slot);

	/// Waits for the device to go idle first.
	void flush_all();

  private:
	void dispatch(const Deletion::ResourceVariant &resource);

	VulkanDevice &m_device;
	Uint32 m_current_slot = 0;
	std::array<std::vector<Deletion::ResourceVariant>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_buckets;
};

} // namespace Aquila::RHI
