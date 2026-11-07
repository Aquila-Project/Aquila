#ifndef AQUILA_VULKAN_TYPES_H
#define AQUILA_VULKAN_TYPES_H

#include "GraphicsPCH.h"
#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::RHI {

struct VkSwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR m_surface_capabilities;
	std::vector<VkSurfaceFormatKHR> m_formats;
	std::vector<VkPresentModeKHR> m_present_modes;
};

struct BufferAllocation {
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VmaAllocationInfo info = {};
	void *mapped_ptr = nullptr;

	[[nodiscard]] bool is_valid() const { return buffer != VK_NULL_HANDLE; }
	[[nodiscard]] bool is_mapped() const { return mapped_ptr != nullptr; }
};

struct ImageAllocation {
	VkImage image = VK_NULL_HANDLE;
	VmaAllocation allocation = nullptr;
	VmaAllocationInfo info = {};
	void *mapped_ptr = nullptr;
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkExtent3D extent = {};
	uint32_t mip_levels = 1;
	uint32_t array_layers = 1;
};

/// A one-off command buffer, freed when its frame slot comes back around.
struct PendingCommandBuffer {
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkCommandPool pool = VK_NULL_HANDLE;
};

struct VkQueueFamilyIndices {
	std::optional<Uint32> m_graphics_family;
	std::optional<Uint32> m_present_family;
	std::optional<Uint32> m_compute_family;
	std::optional<Uint32> m_transfer_family;

	[[nodiscard]] bool is_complete() const {
		return m_graphics_family.has_value() && m_present_family.has_value() && m_compute_family.has_value() &&
			m_transfer_family.has_value();
	}
};

} // namespace Aquila::RHI
#endif
