#include "Aquila/RHI/Vulkan/VulkanSwapchain.h"
#include "Aquila/RHI/DeletionQueue.h"
#include "Aquila/Foundation/Log.h"
#include "Aquila/RHI/Vulkan/VulkanDevice.h"
#include "Aquila/RHI/Vulkan/VulkanDeletionQueue.h"
#include "Aquila/RHI/Vulkan/VulkanFormatUtils.h"

namespace Aquila::RHI {

VulkanSwapchain::VulkanSwapchain(VulkanDevice &device, VkExtent2D extent, bool vsync)
	: m_device(device), m_window_extent(extent), m_v_sync_enabled(vsync), m_surface(device.get_surface()) {
	initialize();
}

VulkanSwapchain::VulkanSwapchain(VulkanDevice &device, VkExtent2D extent, bool vsync, Ref<VulkanSwapchain> previous)
	: m_device(device), m_window_extent(extent), m_v_sync_enabled(vsync), m_surface(device.get_surface()),
	  m_old_swapchain(std::move(previous)) {
	initialize();
	m_old_swapchain = nullptr;
}

VulkanSwapchain::VulkanSwapchain(VulkanDevice &device, VkExtent2D extent, bool vsync, VkSurfaceKHR surface,
								 bool owns_surface)
	: m_device(device), m_window_extent(extent), m_v_sync_enabled(vsync), m_surface(surface), m_owns_surface(owns_surface) {
	initialize();
}

VulkanSwapchain::~VulkanSwapchain() {
	VkDevice dev = m_device.get_device();

	// Application::~Application calls WaitIdle() (vkDeviceWaitIdle) before destroying the
	// swapchain, so all queues are already drained and all fences are in the signaled state.
	for (auto &pending : m_pending_cmd_bufs) {
		for (auto &p : pending) {
			vkFreeCommandBuffers(dev, p.pool, 1, &p.cmd);
		}
		pending.clear();
	}

	for (auto *view : m_image_views) {
		if (view != VK_NULL_HANDLE) {
			vkDestroyImageView(dev, view, nullptr);
		}
	}
	m_image_views.clear();

	for (auto *view : m_depth_image_views) {
		if (view != VK_NULL_HANDLE) {
			vkDestroyImageView(dev, view, nullptr);
		}
	}
	m_depth_image_views.clear();

	for (auto &alloc : m_depth_allocations) {
		vmaDestroyImage(m_device.get_allocator(), alloc.image, alloc.allocation);
	}
	m_depth_allocations.clear();

	if (m_swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(dev, m_swapchain, nullptr);
		m_swapchain = VK_NULL_HANDLE;
	}

	if (m_owns_surface && m_surface != VK_NULL_HANDLE) {
		m_device.destroy_surface_handle(m_surface);
		m_surface = VK_NULL_HANDLE;
	}

	auto &deletion_queue = m_device.get_deletion_queue();

	for (auto *sem : m_image_available_semaphores) {
		if (sem != VK_NULL_HANDLE) {
			deletion_queue.queue_deletion(sem);
		}
	}
	for (auto *sem : m_render_finished_semaphores) {
		if (sem != VK_NULL_HANDLE) {
			deletion_queue.queue_deletion(sem);
		}
	}
	for (auto *fence : m_in_flight_fences) {
		if (fence != VK_NULL_HANDLE) {
			vkDestroyFence(dev, fence, nullptr);
		}
	}
}

void VulkanSwapchain::initialize() {
	create_swapchain();
	create_image_views();
	create_depth_resources();
	create_sync_objects();
}

void VulkanSwapchain::create_swapchain(VkSwapchainKHR old_handle) {
	VkSwapChainSupportDetails support = m_device.get_swap_chain_support(m_surface);

	VkSurfaceFormatKHR surface_format = choose_swap_surface_format(support.m_formats);
	VkPresentModeKHR present_mode = choose_swap_present_mode(support.m_present_modes);
	VkExtent2D extent = choose_swap_extent(support.m_surface_capabilities);

	Uint32 image_count = support.m_surface_capabilities.minImageCount + 1;
	if (support.m_surface_capabilities.maxImageCount > 0 && image_count > support.m_surface_capabilities.maxImageCount) {
		image_count = support.m_surface_capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = m_surface;
	create_info.minImageCount = image_count;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageExtent = extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	VkQueueFamilyIndices indices = m_device.find_physical_qf();
	Uint32 queue_family_indices[] = { indices.m_graphics_family.value(), indices.m_present_family.value() };

	if (indices.m_graphics_family != indices.m_present_family) {
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_family_indices;
	} else {
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	create_info.preTransform = support.m_surface_capabilities.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = present_mode;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain =
		old_handle != VK_NULL_HANDLE ? old_handle : (m_old_swapchain ? m_old_swapchain->m_swapchain : VK_NULL_HANDLE);

	AQUILA_VULKAN_CHECK(vkCreateSwapchainKHR(m_device.get_device(), &create_info, nullptr, &m_swapchain));

	vkGetSwapchainImagesKHR(m_device.get_device(), m_swapchain, &image_count, nullptr);
	m_images.resize(image_count);
	vkGetSwapchainImagesKHR(m_device.get_device(), m_swapchain, &image_count, m_images.data());

	m_image_format = surface_format.format;
	m_extent = extent;
}

void VulkanSwapchain::create_image_views() {
	m_image_views.resize(m_images.size());

	for (size_t i = 0; i < m_images.size(); i++) {
		VkImageViewCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image = m_images[i];
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		create_info.format = m_image_format;
		create_info.components = { .r = VK_COMPONENT_SWIZZLE_IDENTITY,
								  .g = VK_COMPONENT_SWIZZLE_IDENTITY,
								  .b = VK_COMPONENT_SWIZZLE_IDENTITY,
								  .a = VK_COMPONENT_SWIZZLE_IDENTITY };
		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;

		AQUILA_VULKAN_CHECK(vkCreateImageView(m_device.get_device(), &create_info, nullptr, &m_image_views[i]));
	}
}

void VulkanSwapchain::create_depth_resources() {
	m_depth_format = find_depth_format();

	m_depth_allocations.resize(m_images.size());
	m_depth_image_views.resize(m_images.size());

	for (size_t i = 0; i < m_images.size(); i++) {
		m_depth_allocations[i] = m_device.create_image<MemoryDomain::GpuOnly>(
			m_extent.width, m_extent.height, m_depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, 1, 1,
			VK_SAMPLE_COUNT_1_BIT, ("SwapchainDepth_" + std::to_string(i)).c_str());

		VkImageViewCreateInfo view_info{};
		view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_info.image = m_depth_allocations[i].image;
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = m_depth_format;
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.levelCount = 1;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount = 1;

		AQUILA_VULKAN_CHECK(vkCreateImageView(m_device.get_device(), &view_info, nullptr, &m_depth_image_views[i]));
	}
}

void VulkanSwapchain::create_sync_objects() {
	m_image_available_semaphores.resize(SharedConstants::MAX_FRAMES_IN_FLIGHT);
	m_in_flight_fences.resize(SharedConstants::MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo sem_info{};
	sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	// Create fences pre-signaled so AcquireNextImage's first wait returns immediately.
	VkFenceCreateInfo fence_info{};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
		AQUILA_VULKAN_CHECK(vkCreateSemaphore(m_device.get_device(), &sem_info, nullptr, &m_image_available_semaphores[i]));
		AQUILA_VULKAN_CHECK(vkCreateFence(m_device.get_device(), &fence_info, nullptr, &m_in_flight_fences[i]));
	}

	create_render_finished_semaphores();
}

void VulkanSwapchain::create_render_finished_semaphores() {
	VkDevice dev = m_device.get_device();
	for (auto *sem : m_render_finished_semaphores) {
		if (sem != VK_NULL_HANDLE) {
			vkDestroySemaphore(dev, sem, nullptr);
		}
	}

	VkSemaphoreCreateInfo sem_info{};
	sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	m_render_finished_semaphores.assign(m_images.size(), VK_NULL_HANDLE);
	for (auto &sem : m_render_finished_semaphores) {
		AQUILA_VULKAN_CHECK(vkCreateSemaphore(dev, &sem_info, nullptr, &sem));
	}
}

void VulkanSwapchain::wait_for_frame_slot(Uint32 slot) {
	AQUILA_VULKAN_CHECK(vkWaitForFences(m_device.get_device(), 1, &m_in_flight_fences[slot], VK_TRUE, UINT64_MAX));
}

void VulkanSwapchain::discard_unconsumed_acquire(Uint32 slot) {
	if (!m_acquire_pending[slot]) {
		return;
	}

	Aquila::Foundation::log_warning("VulkanSwapchain: frame slot {} acquired an image but never submitted; "
									"draining its acquire semaphore",
									slot);

	VkDevice dev = m_device.get_device();
	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &m_image_available_semaphores[slot];
	submit_info.pWaitDstStageMask = &wait_stage;

	AQUILA_VULKAN_CHECK(vkResetFences(dev, 1, &m_in_flight_fences[slot]));
	m_device.submit_to_graphics_queue(&submit_info, m_in_flight_fences[slot]);
	AQUILA_VULKAN_CHECK(vkWaitForFences(dev, 1, &m_in_flight_fences[slot], VK_TRUE, UINT64_MAX));

	m_acquire_pending[slot] = false;
}

void VulkanSwapchain::release_frame_slot_resources(Uint32 slot) {
	VkDevice dev = m_device.get_device();
	for (auto &pending : m_pending_cmd_bufs[slot]) {
		vkFreeCommandBuffers(dev, pending.pool, 1, &pending.cmd);
	}
	m_pending_cmd_bufs[slot].clear();
}

void VulkanSwapchain::begin_device_frame(Uint32 slot) {
	m_device.get_deletion_queue().flush(slot);
	m_device.reset_frame_command_pool(slot);
	m_device.get_deletion_queue().set_current_slot(slot);
}

bool VulkanSwapchain::acquire_next_image(Uint32 &out_image_index, bool drive_device_frame) {
	const Uint32 slot = m_next_frame_slot;

	wait_for_frame_slot(slot);
	discard_unconsumed_acquire(slot);
	release_frame_slot_resources(slot);

	if (drive_device_frame) {
		begin_device_frame(slot);
	}

	const VkResult result = vkAcquireNextImageKHR(m_device.get_device(), m_swapchain, UINT64_MAX,
												  m_image_available_semaphores[slot], VK_NULL_HANDLE, &out_image_index);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		m_needs_resize = true;
		return false;
	}
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		Aquila::Foundation::log_error("VulkanSwapchain: vkAcquireNextImageKHR failed with VkResult {}",
									  static_cast<Int32>(result));
		return false;
	}
	if (result == VK_SUBOPTIMAL_KHR) {
		m_needs_resize = true;
	}

	m_acquire_pending[slot] = true;
	m_current_frame_slot = slot;
	m_next_frame_slot = (slot + 1) % SharedConstants::MAX_FRAMES_IN_FLIGHT;
	return true;
}

TextureFormat VulkanSwapchain::get_format() const {
	return from_vk_format(m_image_format);
}

VkResult VulkanSwapchain::present_image(Uint32 image_index, VkSemaphore render_finished_semaphore) {
	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &render_finished_semaphore;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &m_swapchain;
	present_info.pImageIndices = &image_index;

	const VkResult result = m_device.present_to_queue(&present_info);

	if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
		m_needs_resize = true;
	} else if (result != VK_SUCCESS) {
		Aquila::Foundation::log_error("VulkanSwapchain: vkQueuePresentKHR failed with VkResult {}",
									  static_cast<Int32>(result));
	}
	return result;
}

void VulkanSwapchain::destroy_image_resources() {
	for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
		m_device.get_deletion_queue().flush(i);
	}

	VkDevice dev = m_device.get_device();

	for (auto &pending : m_pending_cmd_bufs) {
		for (auto &p : pending) {
			vkFreeCommandBuffers(dev, p.pool, 1, &p.cmd);
		}
		pending.clear();
	}

	for (auto *view : m_image_views) {
		if (view != VK_NULL_HANDLE) {
			vkDestroyImageView(dev, view, nullptr);
		}
	}
	m_image_views.clear();

	for (auto *view : m_depth_image_views) {
		if (view != VK_NULL_HANDLE) {
			vkDestroyImageView(dev, view, nullptr);
		}
	}
	m_depth_image_views.clear();

	for (auto &alloc : m_depth_allocations) {
		vmaDestroyImage(m_device.get_allocator(), alloc.image, alloc.allocation);
	}
	m_depth_allocations.clear();
}

void VulkanSwapchain::defer_cmd_buf_free(Uint32 frame_index, VkCommandBuffer cmd, VkCommandPool pool) {
	m_pending_cmd_bufs[frame_index].push_back({ cmd, pool });
}

void VulkanSwapchain::resize(Uint32 width, Uint32 height) {
	m_window_extent = { .width = width, .height = height };

	m_device.wait_idle();

	for (Uint32 slot = 0; slot < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++slot) {
		discard_unconsumed_acquire(slot);
	}

	destroy_image_resources();

	VkSwapchainKHR retired_swapchain = m_swapchain;
	m_swapchain = VK_NULL_HANDLE;
	create_swapchain(retired_swapchain);
	vkDestroySwapchainKHR(m_device.get_device(), retired_swapchain, nullptr);

	create_image_views();
	create_depth_resources();
	create_render_finished_semaphores();

	m_needs_resize = false;
	m_next_frame_slot = 0;
	m_current_frame_slot = 0;
}

VkFormat VulkanSwapchain::find_depth_format() {
	return m_device.find_supported_format(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkSurfaceFormatKHR VulkanSwapchain::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR> &available_formats) {
	const VkFormat preferred[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };

	for (VkFormat wanted : preferred) {
		for (const auto &format : available_formats) {
			if (format.format == wanted && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return format;
			}
		}
	}

	for (VkFormat wanted : preferred) {
		for (const auto &format : available_formats) {
			if (format.format == wanted) {
				return format;
			}
		}
	}

	return available_formats[0];
}

VkPresentModeKHR VulkanSwapchain::choose_swap_present_mode(const std::vector<VkPresentModeKHR> &available_present_modes) {
	if (m_v_sync_enabled) {
		Aquila::Foundation::log_info("VulkanSwapchain: present_mode=FIFO (vsync=true)");
		return VK_PRESENT_MODE_FIFO_KHR;
	}
	for (const auto &mode : available_present_modes) {
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
			Aquila::Foundation::log_info("VulkanSwapchain: present_mode=MAILBOX (vsync=false)");
			return mode;
		}
	}
	for (const auto &mode : available_present_modes) {
		if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
			Aquila::Foundation::log_info("VulkanSwapchain: present_mode=IMMEDIATE (vsync=false, MAILBOX unavailable)");
			return mode;
		}
	}
	Aquila::Foundation::log_warning(
		"VulkanSwapchain: present_mode=FIFO (vsync=false requested but MAILBOX/IMMEDIATE unavailable — vsync active)");
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::choose_swap_extent(const VkSurfaceCapabilitiesKHR &capabilities) const {
	if (capabilities.currentExtent.width != std::numeric_limits<Uint32>::max()) {
		return capabilities.currentExtent;
	}
	VkExtent2D actual = m_window_extent;
	actual.width = std::clamp(actual.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	actual.height = std::clamp(actual.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	return actual;
}

} // namespace Aquila::RHI
