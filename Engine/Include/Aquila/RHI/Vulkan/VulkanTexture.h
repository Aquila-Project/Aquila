#ifndef AQUILA_VULKAN_TEXTURE_H
#define AQUILA_VULKAN_TEXTURE_H

#include "GraphicsPCH.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/RHI/Backend/IRHITexture.h"
#include "Aquila/RHI/Backend/RHITypes.h"
#include "Aquila/RHI/Vulkan/VulkanTypes.h"

namespace Aquila::RHI {

class VulkanDevice;

class VulkanTexture final : public IRHITexture {
  public:
	VulkanTexture(VulkanDevice &device, const TextureDesc &desc);
	~VulkanTexture() override;

	AQUILA_NONCOPYABLE(VulkanTexture);

	void destroy_immediate() override;
	[[nodiscard]] bool is_ready() const override { return m_image_view != VK_NULL_HANDLE; }

	// IRHITexture
	[[nodiscard]] Uint32 get_width() const override { return m_desc.width; }
	[[nodiscard]] Uint32 get_height() const override { return m_desc.height; }
	[[nodiscard]] Uint32 get_mip_levels() const override { return m_desc.mip_levels; }
	[[nodiscard]] Uint32 get_array_layers() const override { return m_desc.array_layers; }
	[[nodiscard]] TextureFormat get_format() const override { return m_desc.format; }
	[[nodiscard]] SampleCount get_sample_count() const override { return m_desc.samples; }
	[[nodiscard]] const TextureDesc &get_desc() const override { return m_desc; }

	// Vulkan-specific accessors (used by rendering systems and descriptor writers)
	[[nodiscard]] VkImage get_image() const { return m_image_allocation.image; }
	[[nodiscard]] VkImageView get_image_view() const { return m_image_view; }
	[[nodiscard]] VkSampler get_sampler() const { return m_sampler; }
	[[nodiscard]] const ImageAllocation &get_allocation() const { return m_image_allocation; }
	[[nodiscard]] VkDescriptorImageInfo get_descriptor_image_info() const;
	[[nodiscard]] VkImageView get_attachment_view(Uint32 mip_level, Uint32 layer);

  private:
	void create_image_view();
	void create_sampler();

	VulkanDevice &m_device;
	TextureDesc m_desc;
	ImageAllocation m_image_allocation{};
	VkImageView m_image_view = VK_NULL_HANDLE;
	std::vector<VkImageView> m_image_views; // Used mostly for cubemaps
	VkSampler m_sampler = VK_NULL_HANDLE;
};

} // namespace Aquila::RHI
#endif
