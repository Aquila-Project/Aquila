#include "Aquila/RHI/Vulkan/VulkanDeletionQueue.h"
#include "Aquila/RHI/Vulkan/VulkanDevice.h"

namespace Aquila::RHI {

DeletionQueue::DeletionQueue(VulkanDevice &device) : m_device(device) {}

DeletionQueue::~DeletionQueue() {
	flush_all();
}

void DeletionQueue::set_current_slot(Uint32 slot) {
	AQUILA_ASSERT(slot < SharedConstants::MAX_FRAMES_IN_FLIGHT, "DeletionQueue slot out of range");
	m_current_slot = slot;
}

void DeletionQueue::queue_deletion(const Deletion::ResourceVariant &resource) {
	m_buckets[m_current_slot].push_back(resource);
}

void DeletionQueue::destroy_now(const Deletion::ResourceVariant &resource) {
	dispatch(resource);
}

void DeletionQueue::flush(Uint32 slot) {
	AQUILA_ASSERT(slot < SharedConstants::MAX_FRAMES_IN_FLIGHT, "DeletionQueue slot out of range");
	for (auto &resource : m_buckets[slot]) {
		dispatch(resource);
	}
	m_buckets[slot].clear();
}

void DeletionQueue::flush_all() {
	AQUILA_ASSERT(m_device.get_device() != VK_NULL_HANDLE, "DeletionQueue::FlushAll — device is null");
	m_device.wait_idle();
	for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
		flush(i);
	}
}

void DeletionQueue::dispatch(const Deletion::ResourceVariant &resource) {
	VkDevice device = m_device.get_device();

	std::visit(
		[&](auto &&res) {
			using T = std::decay_t<decltype(res)>;

			if constexpr (std::is_same_v<T, VkPipeline>) {
				vkDestroyPipeline(device, res, nullptr);
			} else if constexpr (std::is_same_v<T, VkShaderModule>) {
				vkDestroyShaderModule(device, res, nullptr);
			} else if constexpr (std::is_same_v<T, VkPipelineLayout>) {
				vkDestroyPipelineLayout(device, res, nullptr);
			} else if constexpr (std::is_same_v<T, VkPipelineCache>) {
				vkDestroyPipelineCache(device, res, nullptr);
			} else if constexpr (std::is_same_v<T, VkImageView>) {
				vkDestroyImageView(device, res, nullptr);
			} else if constexpr (std::is_same_v<T, VkDeviceMemory>) {
				vkFreeMemory(device, res, nullptr);
			} else if constexpr (std::is_same_v<T, VkSampler>) {
				vkDestroySampler(device, res, nullptr);
			} else if constexpr (std::is_same_v<T, VkDescriptorSetLayout>) {
				vkDestroyDescriptorSetLayout(device, res, nullptr);
			} else if constexpr (std::is_same_v<T, VkRenderPass>) {
				vkDestroyRenderPass(device, res, nullptr);
			} else if constexpr (std::is_same_v<T, VkFramebuffer>) {
				vkDestroyFramebuffer(device, res, nullptr);
			} else if constexpr (std::is_same_v<T, VkSemaphore>) {
				vkDestroySemaphore(device, res, nullptr);
			} else if constexpr (std::is_same_v<T, Deletion::VmaImageDeletion>) {
				vmaDestroyImage(m_device.get_allocator(), res.image, res.allocation);
			} else if constexpr (std::is_same_v<T, Deletion::VmaBufferDeletion>) {
				vmaDestroyBuffer(m_device.get_allocator(), res.buffer, res.allocation);
			} else {
				AQUILA_ASSERT(false, "DeletionQueue::Dispatch — unhandled resource type");
			}
		},
		resource);
}

} // namespace Aquila::RHI
