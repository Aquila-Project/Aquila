#ifndef AQUILA_VULKAN_BUFFER_H
#define AQUILA_VULKAN_BUFFER_H

#include "GraphicsPCH.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/RHI/Backend/IRHIBuffer.h"
#include "Aquila/RHI/Vulkan/VulkanTypes.h"

namespace Aquila::RHI {

class VulkanDevice;

/// Every domain except GpuOnly stays mapped for its whole life.
/// The *_index functions work on one padded instance, see BufferDesc::instance_count.
class VulkanBuffer final : public IRHIBuffer {
  public:
	VulkanBuffer(VulkanDevice &device, const std::string &debug_name, VkDeviceSize instance_size, uint32_t instance_count,
				 VkBufferUsageFlags usage_flags, MemoryDomain domain, VkDeviceSize min_offset_alignment);
	~VulkanBuffer() override;

	AQUILA_NONCOPYABLE(VulkanBuffer);
	AQUILA_NONMOVEABLE(VulkanBuffer);

	// IRHIBuffer
	void write(const void *data, Uint64 size, Uint64 offset = 0) override;
	void *map() override;
	void unmap() override;
	void flush(Uint64 size = 0, Uint64 offset = 0) override;
	void invalidate(Uint64 size = 0, Uint64 offset = 0) override;

	void destroy_immediate() override;

	[[nodiscard]] Uint64 get_size() const override { return m_buffer_size; }
	[[nodiscard]] Uint32 get_instance_count() const override { return m_instance_count; }
	[[nodiscard]] bool is_mapped() const override { return m_mapped_ptr != nullptr; }

	// Extended API (used internally by other Vulkan classes)
	[[nodiscard]] VkResult Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;
	[[nodiscard]] VkResult Invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;
	[[nodiscard]] VkDescriptorBufferInfo descriptor_info(VkDeviceSize size = VK_WHOLE_SIZE,
														VkDeviceSize offset = 0) const;
	[[nodiscard]] VkDescriptorBufferInfo descriptor_info_for_index(int index) const;
	void write_to_index(const void *data, int index) const;
	[[nodiscard]] VkResult flush_index(int index) const;
	[[nodiscard]] VkResult invalidate_index(int index) const;

	[[nodiscard]] VkBuffer get_buffer() const { return m_buffer; }
	[[nodiscard]] void *get_mapped_memory() const { return m_mapped_ptr; }
	[[nodiscard]] VkDeviceSize get_instance_size() const { return m_instance_size; }
	[[nodiscard]] VkDeviceSize get_alignment_size() const { return m_alignment_size; }
	[[nodiscard]] VkBufferUsageFlags get_usage_flags() const { return m_usage_flags; }

	static VkDeviceSize get_alignment(VkDeviceSize instance_size, VkDeviceSize min_offset_alignment);

  private:
	VulkanDevice &m_device;
	VkBuffer m_buffer = VK_NULL_HANDLE;
	VmaAllocation m_allocation = VK_NULL_HANDLE;
	void *m_mapped_ptr = nullptr;
	bool m_persistent_map = false;

	VkDeviceSize m_buffer_size = 0;
	VkDeviceSize m_instance_size = 0;
	VkDeviceSize m_alignment_size = 0;
	uint32_t m_instance_count = 0;
	VkBufferUsageFlags m_usage_flags = 0;
	MemoryDomain m_domain;
};

} // namespace Aquila::RHI
#endif
