#include "Aquila/RHI/Vulkan/VulkanTexture.h"
#include "Aquila/RHI/Vulkan/VulkanDeletionQueue.h"

#include "Aquila/RHI/Vulkan/VulkanDevice.h"
#include "Aquila/RHI/Vulkan/VulkanFormatUtils.h"

namespace Aquila::RHI {

VulkanTexture::VulkanTexture(VulkanDevice &device, const TextureDesc &desc) : m_device(device), m_desc(desc) {
	VkImageCreateFlags flags = 0;
	if (desc.view_type == TextureViewType::Cube || desc.view_type == TextureViewType::CubeArray) {
		flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}

	m_image_allocation = m_device.create_image<MemoryDomain::GpuOnly>(
		desc.width, desc.height, to_vk_format(desc.format), to_vk_image_usage(desc.usage), flags, desc.mip_levels,
		desc.array_layers, to_vk_sample_count(desc.samples), desc.debug_name.c_str());

	create_image_view();
	create_sampler();

	m_device.set_object_debug_name(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<Uint64>(m_image_allocation.image),
								   (desc.debug_name + "_Image").c_str());
	m_device.set_object_debug_name(VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<Uint64>(m_image_view),
								   (desc.debug_name + "_ImageView").c_str());
	m_device.set_object_debug_name(VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<Uint64>(m_sampler),
								   (desc.debug_name + "_Sampler").c_str());
}

VulkanTexture::~VulkanTexture() {
	auto &queue = m_device.get_deletion_queue();

	if (m_image_view != VK_NULL_HANDLE) {
		queue.queue_deletion(m_image_view);
	}

	if (m_image_allocation.image != VK_NULL_HANDLE) {
		queue.queue_deletion(Deletion::VmaImageDeletion{ .image = m_image_allocation.image,
														 .allocation = m_image_allocation.allocation });
	}
}

void VulkanTexture::destroy_immediate() {
	if (m_image_view != VK_NULL_HANDLE) {
		vkDestroyImageView(m_device.get_device(), m_image_view, nullptr);
		m_image_view = VK_NULL_HANDLE;
	}
	vmaDestroyImage(m_device.get_allocator(), m_image_allocation.image, m_image_allocation.allocation);
	m_image_allocation.image = VK_NULL_HANDLE;
	m_image_allocation.allocation = VK_NULL_HANDLE;
}

VkDescriptorImageInfo VulkanTexture::get_descriptor_image_info() const {
	VkDescriptorImageInfo info{};
	info.imageLayout = is_depth_format(m_desc.format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
													  : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	info.imageView = m_image_view;
	info.sampler = m_sampler;
	return info;
}

void VulkanTexture::create_image_view() {
	bool is_depth = is_depth_format(m_desc.format);

	VkImageViewCreateInfo view_info{};
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = m_image_allocation.image;
	view_info.viewType = to_vk_image_view_type(m_desc.view_type);
	view_info.format = to_vk_format(m_desc.format);
	view_info.components = to_vk_component_mapping(m_desc.swizzle);
	view_info.subresourceRange.aspectMask = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = m_desc.mip_levels;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = m_desc.array_layers;

	AQUILA_VULKAN_CHECK(vkCreateImageView(m_device.get_device(), &view_info, nullptr, &m_image_view));
}

void VulkanTexture::create_sampler() {
	m_sampler = m_device.get_or_create_sampler(m_desc.sampler);
}

} // namespace Aquila::RHI
