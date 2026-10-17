#ifndef AQUILA_VULKAN_SWAPCHAIN_H
#define AQUILA_VULKAN_SWAPCHAIN_H

#include "GraphicsPCH.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/RHI/Backend/IRHISwapchain.h"
#include "Aquila/RHI/Vulkan/VulkanTypes.h"

namespace Aquila::RHI {

class VulkanDevice;

class VulkanSwapchain final : public IRHISwapchain {
  public:
	VulkanSwapchain(VulkanDevice &device, VkExtent2D extent, bool vsync);
	VulkanSwapchain(VulkanDevice &device, VkExtent2D extent, bool vsync, Ref<VulkanSwapchain> previous);
	/// For a secondary window's surface. With `owns_surface` the swapchain destroys it.
	VulkanSwapchain(VulkanDevice &device, VkExtent2D extent, bool vsync, VkSurfaceKHR surface, bool owns_surface);
	~VulkanSwapchain() override;

	AQUILA_NONCOPYABLE(VulkanSwapchain);

	// IRHISwapchain semaphores managed internally per frame
	bool acquire_next_image(Uint32 &out_image_index, bool drive_device_frame) override;
	[[nodiscard]] bool needs_resize() const override { return m_needs_resize; }
	void resize(Uint32 width, Uint32 height) override;

	[[nodiscard]] Uint32 get_width() const override { return m_extent.width; }
	[[nodiscard]] Uint32 get_height() const override { return m_extent.height; }
	[[nodiscard]] TextureFormat get_format() const override;
	[[nodiscard]] Uint32 get_image_count() const override { return static_cast<Uint32>(m_images.size()); }

	// Vulkan-specific accessors (render loops, ImGui, etc.)
	[[nodiscard]] VkFormat get_image_format() const { return m_image_format; }
	[[nodiscard]] VkFormat get_depth_format() const { return m_depth_format; }
	[[nodiscard]] VkExtent2D get_extent() const { return m_extent; }
	[[nodiscard]] VkImage get_image(Uint32 index) const { return m_images[index]; }
	[[nodiscard]] VkImageView get_image_view(Uint32 index) const { return m_image_views[index]; }
	[[nodiscard]] VkImageView get_depth_image_view(Uint32 index) const { return m_depth_image_views[index]; }
	[[nodiscard]] VkImage get_depth_image(Uint32 index) const { return m_depth_allocations[index].image; }

	// Semaphore accessors for external sync (render loop)
	[[nodiscard]] VkSemaphore get_image_available_semaphore(Uint32 frame_index) const {
		return m_image_available_semaphores[frame_index];
	}
	[[nodiscard]] VkSemaphore get_render_finished_semaphore(Uint32 image_index) const {
		return m_render_finished_semaphores[image_index];
	}
	[[nodiscard]] VkFence get_in_flight_fence(Uint32 frame_index) const { return m_in_flight_fences[frame_index]; }

	// Queue a command buffer for deferred free once frameIndex's fence is waited on next.
	void defer_cmd_buf_free(Uint32 frame_index, VkCommandBuffer cmd, VkCommandPool pool);

	VkResult present_image(Uint32 image_index, VkSemaphore render_finished_semaphore);

	[[nodiscard]] Uint32 get_current_frame_slot() const override { return m_current_frame_slot; }

	void notify_frame_submitted(Uint32 slot) { m_acquire_pending[slot] = false; }

	[[nodiscard]] F32 aspect_ratio() const {
		return static_cast<F32>(m_extent.width) / static_cast<F32>(m_extent.height);
	}
	[[nodiscard]] VkFormat find_depth_format();

  private:
	void initialize();
	void create_swapchain(VkSwapchainKHR old_handle = VK_NULL_HANDLE);
	void create_image_views();
	void create_depth_resources();
	void create_sync_objects();
	void create_render_finished_semaphores();
	void destroy_image_resources();

	void wait_for_frame_slot(Uint32 slot);
	void discard_unconsumed_acquire(Uint32 slot);
	void release_frame_slot_resources(Uint32 slot);
	void begin_device_frame(Uint32 slot);

	VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR> &available_formats);
	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR> &available_present_modes);
	[[nodiscard]] VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR &capabilities) const;

	VulkanDevice &m_device;
	VkExtent2D m_window_extent;
	bool m_v_sync_enabled = false;
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	bool m_owns_surface = false;
	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
	Ref<VulkanSwapchain> m_old_swapchain;

	VkFormat m_image_format{};
	VkFormat m_depth_format{};
	VkExtent2D m_extent{};

	std::vector<VkImage> m_images;
	std::vector<VkImageView> m_image_views;

	std::vector<ImageAllocation> m_depth_allocations;
	std::vector<VkImageView> m_depth_image_views;

	struct PendingCmdBuf {
		VkCommandBuffer cmd;
		VkCommandPool pool;
	};

	std::vector<VkSemaphore> m_image_available_semaphores;
	std::vector<VkSemaphore> m_render_finished_semaphores;
	std::vector<VkFence> m_in_flight_fences;
	std::array<std::vector<PendingCmdBuf>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_pending_cmd_bufs;

	std::array<bool, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_acquire_pending{};

	Uint32 m_next_frame_slot = 0;
	Uint32 m_current_frame_slot = 0;

	bool m_needs_resize = false;
};

} // namespace Aquila::RHI
#endif
