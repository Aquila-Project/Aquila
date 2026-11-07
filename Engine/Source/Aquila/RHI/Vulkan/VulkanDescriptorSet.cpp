#include "Aquila/RHI/Vulkan/VulkanDescriptorSet.h"
#include "Aquila/RHI/Vulkan/VulkanBuffer.h"
#include "Aquila/RHI/Vulkan/VulkanDescriptors.h"
#include "Aquila/RHI/Vulkan/VulkanDevice.h"
#include "Aquila/RHI/Vulkan/VulkanTexture.h"

namespace Aquila::RHI {

VulkanDescriptorSet::VulkanDescriptorSet(VulkanDevice &device, VkDescriptorSet set, VulkanDescriptorSetLayout &layout,
										 VulkanDescriptorPool &pool)
	: m_device(device), m_set(set), m_layout(layout), m_pool(pool) {}

VulkanDescriptorSet::~VulkanDescriptorSet() {
	if (m_set != VK_NULL_HANDLE) {
		m_pool.free_descriptor(m_set);
	}
}

void VulkanDescriptorSet::set_buffer(Uint32 binding, IRHIBuffer &buffer, Uint64 offset, Uint64 range) {
	auto &vk_buf = static_cast<VulkanBuffer &>(buffer);

	m_buffer_infos.push_back({
		.buffer = vk_buf.get_buffer(),
		.offset = offset,
		.range = range == 0 ? VK_WHOLE_SIZE : range,
	});

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = m_set;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.descriptorCount = 1;

	const auto &bindings = m_layout.get_bindings();
	auto it = bindings.find(binding);
	write.descriptorType = (it != bindings.end()) ? it->second.descriptorType : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write.pBufferInfo = nullptr; // fixed up in Flush()

	m_pending_writes.push_back(write);
}

void VulkanDescriptorSet::set_texture(Uint32 binding, IRHITexture &texture) {
	m_image_infos.push_back(static_cast<VulkanTexture &>(texture).get_descriptor_image_info());

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = m_set;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = nullptr; // fixed up in Flush()

	m_pending_writes.push_back(write);
}

void VulkanDescriptorSet::flush() {
	if (m_pending_writes.empty()) {
		return;
	}

	Uint32 buffer_idx = 0;
	Uint32 image_idx = 0;

	for (auto &write : m_pending_writes) {
		if (write.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
			write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
			write.pBufferInfo = &m_buffer_infos[buffer_idx++];
		} else {
			write.pImageInfo = &m_image_infos[image_idx++];
		}
	}

	vkUpdateDescriptorSets(m_device.get_device(), static_cast<Uint32>(m_pending_writes.size()), m_pending_writes.data(), 0,
						   nullptr);

	m_pending_writes.clear();
	m_buffer_infos.clear();
	m_image_infos.clear();
}

} // namespace Aquila::RHI
