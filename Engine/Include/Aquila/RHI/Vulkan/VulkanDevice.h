#ifndef AQUILA_VULKAN_DEVICE_H
#define AQUILA_VULKAN_DEVICE_H

#include "GraphicsPCH.h"
#include "GLFW/glfw3.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/SharedConstants.h"

#include "Aquila/RHI/Backend/IRHIDevice.h"
#include "Aquila/RHI/Backend/RHITypes.h"
#include "Aquila/RHI/Vulkan/VulkanTypes.h"
#include "Aquila/RHI/Vulkan/VulkanCommandList.h"
#include "Aquila/RHI/Vulkan/VulkanFormatUtils.h"

namespace Aquila::RHI {

class DeletionQueue;
class VulkanDescriptorPool;
class VulkanBuffer;
class VulkanTexture;
class VulkanSwapchain;

class VulkanDevice final : public IRHIDevice {
  public:
#ifdef ENABLE_VALIDATION_LAYERS
	constexpr static bool enableValidationLayers = true;
#else
	constexpr static bool enableValidationLayers = false;
#endif

	explicit VulkanDevice(GLFWwindow &native_window);
	~VulkanDevice() override;

	AQUILA_NONCOPYABLE(VulkanDevice);
	AQUILA_NONMOVEABLE(VulkanDevice);

	[[nodiscard]] Unique<IRHIBuffer> create_buffer(const BufferDesc &desc) override;
	[[nodiscard]] Unique<IRHITexture> create_texture(const TextureDesc &desc) override;
	[[nodiscard]] Unique<IRHICommandList> create_command_list(CommandListType type,
															const std::string &name = "") override;
	[[nodiscard]] Unique<IRHICommandList> create_frame_command_list(Uint32 slot) override;
	[[nodiscard]] Unique<IRHISwapchain> create_swapchain(const SwapchainDesc &desc) override;
	[[nodiscard]] Unique<IRHIPipeline> create_graphics_pipeline(const GraphicsPipelineDesc &desc) override;
	[[nodiscard]] Unique<IRHIPipeline> create_compute_pipeline(const ComputePipelineDesc &desc) override;
	[[nodiscard]] Unique<IRHIRenderPass> create_render_pass(const RHI::RenderPassDesc &desc) override;
	[[nodiscard]] Unique<IRHIQueryPool> create_timestamp_pool(Uint32 count) override;

	[[nodiscard]] Unique<IRHIDescriptorSetLayout>
	create_descriptor_set_layout(const DescriptorSetLayoutDesc &desc) override;
	[[nodiscard]] Unique<IRHIDescriptorSet> allocate_descriptor_set(IRHIDescriptorSetLayout &layout) override;
	void copy_buffer(IRHICommandList &cmd, IRHIBuffer &src, IRHIBuffer &dst, Uint64 size, Uint64 src_offset = 0,
					Uint64 dst_offset = 0) override;
	void submit(IRHICommandList &cmd) override;
	void submit_and_wait(IRHICommandList &cmd) override;
	void submit_frame(IRHICommandList &cmd, IRHISwapchain *swapchain, Uint32 image_index) override;
	void present_frame(IRHISwapchain &swapchain, Uint32 image_index,
					  Vec4 clear_color = { 0.0F, 0.0F, 0.0F, 1.0F }) override;
	void wait_idle() override { vkDeviceWaitIdle(m_device); }

	void submit_to_graphics_queue(const VkSubmitInfo *submit_info, VkFence fence);
	void submit_to_compute_queue(const VkSubmitInfo *submit_info, VkFence fence);
	void submit_to_transfer_queue(const VkSubmitInfo *submit_info, VkFence fence);
	VkResult present_to_queue(const VkPresentInfoKHR *present_info);
	void wait_graphics_queue_idle();
	void wait_transfer_queue_idle();

	void reset_frame_command_pool(Uint32 slot);
	void wait() const { vkDeviceWaitIdle(m_device); }

	VkCommandPool get_or_create_thread_local_graphics_pool();

	template <typename Func> void execute_graphics_commands(Func &&func) {
		VkCommandPool pool = get_or_create_thread_local_graphics_pool();
		execute_single_time_commands(pool, m_graphics_queue, m_graphics_queue_mutex, std::forward<Func>(func));
	}

	template <typename Func> void execute_transfer_commands(Func &&func) {
		execute_single_time_commands(m_transfer_command_pool, m_transfer_queue, m_transfer_queue_mutex,
								  std::forward<Func>(func));
	}

	VkFence create_fence(bool signaled = false);
	void wait_for_fence(VkFence fence);
	void destroy_fence(VkFence fence);

	VkFormat find_supported_format(const std::vector<VkFormat> &candidates, VkImageTiling tiling,
								 VkFormatFeatureFlags features);

	void set_object_debug_name(VkObjectType object_type, Uint64 handle, const char *name) const;

	[[nodiscard]] PFN_vkCmdBeginDebugUtilsLabelEXT get_debug_begin_label() const { return m_vk_cmd_begin_debug_utils_label_ext; }
	[[nodiscard]] PFN_vkCmdEndDebugUtilsLabelEXT get_debug_end_label() const { return m_vk_cmd_end_debug_utils_label_ext; }

	void create_graphics_command_pool();
	void create_compute_command_pool();
	void create_transfer_command_pool();
	void create_frame_command_pools();

	template <MemoryDomain Domain>
	BufferAllocation CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, const char *debug_name = nullptr) {
		AQUILA_ASSERT(size > 0, "Buffer size must be > 0");
		AQUILA_ASSERT(m_allocator != VK_NULL_HANDLE, "VMA allocator is null");

		VkBufferCreateInfo buffer_info{};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = size;
		buffer_info.usage = usage;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo alloc_info{};
		if constexpr (Domain == MemoryDomain::GpuOnly) {
			alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			alloc_info.flags = 0;
		} else if constexpr (Domain == MemoryDomain::CpuToGpu) {
			alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
			alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		} else if constexpr (Domain == MemoryDomain::GpuToCpu) {
			alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
			alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		} else if constexpr (Domain == MemoryDomain::CpuOnly) {
			alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
			alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}

		BufferAllocation buffer_allocation{};
		VmaAllocationInfo vma_info{};
		AQUILA_VULKAN_CHECK(vmaCreateBuffer(m_allocator, &buffer_info, &alloc_info, &buffer_allocation.buffer,
											&buffer_allocation.allocation, &vma_info));
		buffer_allocation.info = vma_info;
		buffer_allocation.mapped_ptr = vma_info.pMappedData;

#ifdef AQUILA_DEBUG
		if (debug_name && enableValidationLayers) {
			set_object_debug_name(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(buffer_allocation.buffer), debug_name);
		}
#endif
		return buffer_allocation;
	}

	template <MemoryDomain Domain>
	ImageAllocation create_image(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage,
								uint32_t mip_levels = 1, uint32_t array_layers = 1,
								VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
								const char *debug_name = nullptr) {
		AQUILA_ASSERT(m_allocator != VK_NULL_HANDLE, "VMA allocator is null");
		AQUILA_ASSERT(width > 0 && height > 0, "extent must be > 0");

		constexpr bool is_cpu_accessible = (Domain != MemoryDomain::GpuOnly);
		constexpr VkImageTiling tiling = is_cpu_accessible ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;

		if constexpr (is_cpu_accessible) {
			AQUILA_ASSERT(mip_levels == 1, "Linear tiling does not support mipmaps.");
			AQUILA_ASSERT(samples == VK_SAMPLE_COUNT_1_BIT, "Linear tiling does not support MSAA.");
			AQUILA_ASSERT(array_layers == 1, "Linear tiling array layer support is not guaranteed.");
		}

		VkImageCreateInfo image_info{};
		image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_info.imageType = VK_IMAGE_TYPE_2D;
		image_info.format = format;
		image_info.extent = { .width = width, .height = height, .depth = 1 };
		image_info.mipLevels = mip_levels;
		image_info.arrayLayers = array_layers;
		image_info.samples = samples;
		image_info.tiling = tiling;
		image_info.usage = usage;
		image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_info.initialLayout = is_cpu_accessible ? VK_IMAGE_LAYOUT_PREINITIALIZED : VK_IMAGE_LAYOUT_UNDEFINED;

		VmaAllocationCreateInfo alloc_info{};
		if constexpr (Domain == MemoryDomain::GpuOnly) {
			alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			alloc_info.flags = 0;
			alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		} else if constexpr (Domain == MemoryDomain::CpuToGpu) {
			alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
			alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		} else if constexpr (Domain == MemoryDomain::GpuToCpu) {
			alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
			alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			alloc_info.preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		} else if constexpr (Domain == MemoryDomain::CpuOnly) {
			alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
			alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}

		ImageAllocation image_allocation{};
		VmaAllocationInfo vma_info{};
		AQUILA_VULKAN_CHECK(vmaCreateImage(m_allocator, &image_info, &alloc_info, &image_allocation.image,
										   &image_allocation.allocation, &vma_info));
		image_allocation.info = vma_info;
		image_allocation.mapped_ptr = vma_info.pMappedData;
		image_allocation.format = format;
		image_allocation.extent = { .width = width, .height = height, .depth = 1 };
		image_allocation.mip_levels = mip_levels;
		image_allocation.array_layers = array_layers;

		if constexpr (is_cpu_accessible) {
			VkImageSubresource sub{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
			VkSubresourceLayout layout{};
			vkGetImageSubresourceLayout(m_device, image_allocation.image, &sub, &layout);
			image_allocation.info.size = layout.size;
		}

#ifdef AQUILA_DEBUG
		if (debug_name && enableValidationLayers) {
			set_object_debug_name(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(image_allocation.image), debug_name);
		}
#endif
		return image_allocation;
	}

	VkSampler get_or_create_sampler(const SamplerDesc &desc);
	void destroy_sampler_cache();

	[[nodiscard]] RHI::DeletionQueue &get_deletion_queue() const;
	[[nodiscard]] VkCommandPool get_graphics_command_pool() const { return m_graphics_command_pool; }
	[[nodiscard]] VkCommandPool get_compute_command_pool() const { return m_compute_command_pool; }
	[[nodiscard]] VkCommandPool get_transfer_command_pool() const { return m_transfer_command_pool; }
	[[nodiscard]] VkQueue get_graphics_queue() const { return m_graphics_queue; }
	[[nodiscard]] VkQueue get_present_queue() const { return m_present_queue; }
	[[nodiscard]] VkQueue get_compute_queue() const { return m_compute_queue; }
	[[nodiscard]] VkQueue get_transfer_queue() const { return m_transfer_queue; }
	[[nodiscard]] VkInstance get_instance() const { return m_vulkan_instance; }
	[[nodiscard]] VkDevice &get_device() { return m_device; }
	[[nodiscard]] VkPhysicalDevice get_physical_device() const { return m_physical_device; }
	[[nodiscard]] VkSurfaceKHR get_surface() const { return m_surface; }
	[[nodiscard]] VmaAllocator get_allocator() const { return m_allocator; }
	[[nodiscard]] VkPipelineCache get_pipeline_cache() const { return m_pipeline_cache; }
	[[nodiscard]] VkQueueFamilyIndices find_physical_qf() const { return find_queue_families(m_physical_device); }
	[[nodiscard]] VkSwapChainSupportDetails get_swap_chain_support() const {
		return query_swap_chain_support(m_physical_device, m_surface);
	}
	[[nodiscard]] VkSwapChainSupportDetails get_swap_chain_support(VkSurfaceKHR surface) const {
		return query_swap_chain_support(m_physical_device, surface);
	}

	[[nodiscard]] VkSurfaceKHR create_surface_for_window(GLFWwindow *window) const;
	void destroy_surface_handle(VkSurfaceKHR surface) const;

  private:
	void create_instance();
	void setup_debug_messenger();
	void initialize_vma();
	void pick_physical_device();
	void create_logical_device();
	void create_surface();

	bool is_suitable(VkPhysicalDevice vk_physical_device);
	bool check_device_extension_support(VkPhysicalDevice vk_physical_device) const;
	bool check_validation_layer_support() const;
	std::vector<const char *> get_required_extensions() const;
	VkQueueFamilyIndices find_queue_families(VkPhysicalDevice vk_physical_device) const;
	void submit_swapchain_frame(VulkanCommandList &cmd, VulkanSwapchain &swapchain, Uint32 image_index);
	void submit_offscreen_frame(VulkanCommandList &cmd);
	[[nodiscard]] bool is_frame_managed_pool(VkCommandPool pool) const;
	VkSwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice vk_physical_device, VkSurfaceKHR surface) const;
	void log_device_info() const;

	template <typename Func>
	void execute_single_time_commands(VkCommandPool pool, VkQueue queue, std::mutex &queue_mutex, Func &&func) {
		VkCommandBuffer cmd = nullptr;
		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandPool = pool;
		alloc_info.commandBufferCount = 1;
		vkAllocateCommandBuffers(m_device, &alloc_info, &cmd);

		VkCommandBufferBeginInfo begin_info{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmd, &begin_info);

		std::forward<Func>(func)(cmd);

		vkEndCommandBuffer(cmd);

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cmd;

		VkFence fence = create_fence(false);
		{
			std::lock_guard<std::mutex> lock(queue_mutex);
			vkQueueSubmit(queue, 1, &submit_info, fence);
		}
		wait_for_fence(fence);
		destroy_fence(fence);

		vkFreeCommandBuffers(m_device, pool, 1, &cmd);
	}

	static VkResult create_debug_messenger_ext(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *p_create_info,
											const VkAllocationCallbacks *p_allocator,
											VkDebugUtilsMessengerEXT *p_debug_messenger);
	static void destroy_debug_messenger_ext(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger,
										 const VkAllocationCallbacks *p_allocator);
	static void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT &create_info);
	static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
														VkDebugUtilsMessageTypeFlagsEXT message_type,
														const VkDebugUtilsMessengerCallbackDataEXT *p_callback_data,
														void *p_user_data);
	static const char *get_object_type_name(VkObjectType object_type);

	// Vulkan handles
	VkInstance m_vulkan_instance{};
	VkDebugUtilsMessengerEXT m_debug_messenger{};
	VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
	VkDevice m_device{};
	VkSurfaceKHR m_surface{};
	VmaAllocator m_allocator{};
	VkPhysicalDeviceProperties m_properties{};
	PFN_vkSetDebugUtilsObjectNameEXT m_vk_set_debug_utils_object_name_ext = nullptr;
	PFN_vkCmdBeginDebugUtilsLabelEXT m_vk_cmd_begin_debug_utils_label_ext = nullptr;
	PFN_vkCmdEndDebugUtilsLabelEXT m_vk_cmd_end_debug_utils_label_ext = nullptr;

	// Queues
	VkQueue m_graphics_queue{};
	VkQueue m_present_queue{};
	VkQueue m_compute_queue{};
	VkQueue m_transfer_queue{};

	std::mutex m_graphics_queue_mutex;
	std::mutex m_present_queue_mutex;
	std::mutex m_compute_queue_mutex;
	std::mutex m_transfer_queue_mutex;

	// Command pools
	VkCommandPool m_graphics_command_pool{};
	VkCommandPool m_compute_command_pool{};
	VkCommandPool m_transfer_command_pool{};
	std::mutex m_transfer_command_pool_mutex;
	std::mutex m_graphics_command_pool_mutex;

	struct FrameCommandSlot {
		VkCommandPool pool = VK_NULL_HANDLE;
		VkCommandBuffer cmd = VK_NULL_HANDLE;
	};
	std::array<FrameCommandSlot, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_frame_slots{};

	Unique<RHI::DeletionQueue> m_deletion_queue;

	std::unordered_map<SamplerDesc, VkSampler, SamplerDescHash> m_sampler_cache;

	Unique<VulkanDescriptorPool> m_global_pool;
	void create_global_descriptor_pool();
	void destroy_global_descriptor_pool();

	VkPipelineCache m_pipeline_cache = VK_NULL_HANDLE;
	void create_pipeline_cache();
	void save_pipeline_cache() const;
	[[nodiscard]] std::string pipeline_cache_path() const;

	struct ThreadLocalPool {
		VkCommandPool pool = VK_NULL_HANDLE;
	};
	std::unordered_map<std::thread::id, ThreadLocalPool> m_thread_pools;
	std::mutex m_thread_pool_map_mutex;

	GLFWwindow &m_window_handle;

	struct OffscreenPendingCmdBuf {
		VkCommandBuffer cmd;
		VkCommandPool pool;
	};
	std::array<VkFence, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_offscreen_fences{};
	std::array<std::vector<OffscreenPendingCmdBuf>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_offscreen_pending_cmd_bufs;
	Uint32 m_offscreen_frame_index = 0;

	const std::vector<const char *> VALIDATION_LAYERS = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char *> DEVICE_EXTENSIONS = { VK_KHR_SWAPCHAIN_EXTENSION_NAME,
														 VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
														 VK_KHR_MAINTENANCE1_EXTENSION_NAME };
};

} // namespace Aquila::RHI
#endif
