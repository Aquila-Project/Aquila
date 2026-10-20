#include "Aquila/RHI/Vulkan/VulkanDevice.h"
#include "Aquila/RHI/Vulkan/VulkanQueryPool.h"
#include "Aquila/RHI/Vulkan/VulkanDeletionQueue.h"
#include "Aquila/Foundation/Profiler.h"

#include "Aquila/RHI/Vulkan/VulkanBuffer.h"
#include "Aquila/RHI/Vulkan/VulkanCommandList.h"
#include "Aquila/RHI/Vulkan/VulkanComputePipeline.h"
#include "Aquila/RHI/Vulkan/VulkanDescriptorSet.h"
#include "Aquila/RHI/Vulkan/VulkanDescriptors.h"
#include "Aquila/RHI/Vulkan/VulkanFormatUtils.h"
#include "Aquila/RHI/Vulkan/VulkanPipeline.h"
#include "Aquila/RHI/Vulkan/VulkanRenderPass.h"
#include "Aquila/RHI/Vulkan/VulkanShader.h"
#include "Aquila/RHI/Vulkan/VulkanSwapchain.h"
#include "Aquila/RHI/Vulkan/VulkanTexture.h"
#include "Aquila/RHI/Vulkan/VulkanVertex.h"
#include "Aquila/Platform/Filesystem/Filesystem.h"

#include <vulkan/vulkan_core.h>

namespace Aquila::RHI {

VulkanDevice::VulkanDevice(GLFWwindow &native_window) : m_window_handle(native_window) {
	create_instance();
	create_surface();
	pick_physical_device();
	create_logical_device();

	initialize_vma();

	m_deletion_queue = std::make_unique<RHI::DeletionQueue>(*this);

	create_pipeline_cache();

	create_graphics_command_pool();
	create_compute_command_pool();
	create_transfer_command_pool();
	create_frame_command_pools();
	create_global_descriptor_pool();

	setup_debug_messenger();
	log_device_info();

	VkFenceCreateInfo offscreen_fence_info{};
	offscreen_fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	offscreen_fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
		AQUILA_VULKAN_CHECK(vkCreateFence(m_device, &offscreen_fence_info, nullptr, &m_offscreen_fences[i]));
	}
}

VulkanDevice::~VulkanDevice() {
	wait(); // before killing device wait for it, be gentle

	for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
		for (auto &p : m_offscreen_pending_cmd_bufs[i]) {
			vkFreeCommandBuffers(m_device, p.pool, 1, &p.cmd);
		}
		vkDestroyFence(m_device, m_offscreen_fences[i], nullptr);
	}

	save_pipeline_cache();
	if (m_pipeline_cache != VK_NULL_HANDLE) {
		vkDestroyPipelineCache(m_device, m_pipeline_cache, nullptr);
		m_pipeline_cache = VK_NULL_HANDLE;
	}

	m_deletion_queue.reset();

	destroy_sampler_cache();
	destroy_global_descriptor_pool();

	vmaDestroyAllocator(m_allocator);

	for (auto &[id, threadPool] : m_thread_pools) {
		if (threadPool.pool != VK_NULL_HANDLE) {
			vkDestroyCommandPool(m_device, threadPool.pool, nullptr);
		}
	}

	for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
		vkDestroyCommandPool(m_device, m_frame_slots[i].pool, nullptr);
	}

	vkDestroyCommandPool(m_device, m_graphics_command_pool, nullptr);
	vkDestroyCommandPool(m_device, m_compute_command_pool, nullptr);
	vkDestroyCommandPool(m_device, m_transfer_command_pool, nullptr);

	vkDestroyDevice(m_device, nullptr);

	if (enableValidationLayers) {
		destroy_debug_messenger_ext(m_vulkan_instance, m_debug_messenger, nullptr);
	}

	vkDestroySurfaceKHR(m_vulkan_instance, m_surface, nullptr);
	vkDestroyInstance(m_vulkan_instance, nullptr);

	AQUILA_LOG_DEBUG("VulkanDevice destroyed!");
}

std::string VulkanDevice::pipeline_cache_path() const {
	return Platform::Filesystem::path_user_cache_dir() + "/Aquila/" + Platform::Filesystem::path_executable_name() +
		"/pipeline_cache.bin";
}

void VulkanDevice::create_pipeline_cache() {
	VkPhysicalDeviceProperties props{};
	vkGetPhysicalDeviceProperties(m_physical_device, &props);

	std::vector<char> initial_data;
	std::ifstream file(pipeline_cache_path(), std::ios::binary | std::ios::ate);
	if (file.is_open()) {
		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);
		if (size >= 32) {
			std::vector<char> blob(static_cast<size_t>(size));
			if (file.read(blob.data(), size)) {
				Uint32 header_version = 0;
				Uint32 vendor_id = 0;
				Uint32 device_id = 0;
				std::memcpy(&header_version, blob.data() + 4, sizeof(Uint32));
				std::memcpy(&vendor_id, blob.data() + 8, sizeof(Uint32));
				std::memcpy(&device_id, blob.data() + 12, sizeof(Uint32));

				bool matches = header_version == VK_PIPELINE_CACHE_HEADER_VERSION_ONE &&
							   vendor_id == props.vendorID && device_id == props.deviceID &&
							   std::memcmp(blob.data() + 16, props.pipelineCacheUUID, VK_UUID_SIZE) == 0;
				if (matches) {
					initial_data = std::move(blob);
				} else {
					AQUILA_LOG_INFO("VulkanDevice: pipeline cache on disk is stale, discarding");
				}
			}
		}
	}

	VkPipelineCacheCreateInfo cache_info{};
	cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	cache_info.initialDataSize = initial_data.size();
	cache_info.pInitialData = initial_data.empty() ? nullptr : initial_data.data();
	AQUILA_VULKAN_CHECK(vkCreatePipelineCache(m_device, &cache_info, nullptr, &m_pipeline_cache));

	if (!initial_data.empty()) {
		AQUILA_LOG_INFO("VulkanDevice: loaded pipeline cache ({} bytes)", initial_data.size());
	}
}

void VulkanDevice::save_pipeline_cache() const {
	if (m_pipeline_cache == VK_NULL_HANDLE) {
		return;
	}

	size_t size = 0;
	if (vkGetPipelineCacheData(m_device, m_pipeline_cache, &size, nullptr) != VK_SUCCESS || size == 0) {
		return;
	}

	std::vector<char> blob(size);
	if (vkGetPipelineCacheData(m_device, m_pipeline_cache, &size, blob.data()) != VK_SUCCESS) {
		return;
	}

	const std::string path = pipeline_cache_path();
	std::error_code error;
	std::filesystem::create_directories(std::filesystem::path(path).parent_path(), error);
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (file.is_open()) {
		file.write(blob.data(), static_cast<std::streamsize>(size));
	}
}

Unique<IRHIBuffer> VulkanDevice::create_buffer(const BufferDesc &desc) {
	VkBufferUsageFlags vk_usage = to_vk_buffer_usage(desc.usage);
	if (desc.domain == MemoryDomain::GpuOnly) {
		vk_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}
	return std::make_unique<VulkanBuffer>(*this, desc.debug_name, static_cast<VkDeviceSize>(desc.size),
										  desc.instance_count, vk_usage, desc.domain,
										  static_cast<VkDeviceSize>(desc.min_alignment));
}

Unique<IRHITexture> VulkanDevice::create_texture(const TextureDesc &desc) {
	return std::make_unique<VulkanTexture>(*this, desc);
}

Unique<IRHICommandList> VulkanDevice::create_command_list(CommandListType type, const std::string &name) {
	VkCommandPool pool = VK_NULL_HANDLE;
	switch (type) {
	case CommandListType::Compute:
		pool = m_compute_command_pool;
		break;
	case CommandListType::Transfer:
		pool = m_transfer_command_pool;
		break;
	default:
		pool = m_graphics_command_pool;
		break;
	}
	return std::make_unique<VulkanCommandList>(*this, pool, type, name);
}

Unique<IRHISwapchain> VulkanDevice::create_swapchain(const SwapchainDesc &desc) {
	VkExtent2D extent{ desc.width, desc.height };
	if (desc.native_window_handle != nullptr) {
		VkSurfaceKHR surface = create_surface_for_window(static_cast<GLFWwindow *>(desc.native_window_handle));
		return std::make_unique<VulkanSwapchain>(*this, extent, desc.vsync, surface, true);
	}
	return std::make_unique<VulkanSwapchain>(*this, extent, desc.vsync);
}

VkSurfaceKHR VulkanDevice::create_surface_for_window(GLFWwindow *window) const {
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if (glfwCreateWindowSurface(m_vulkan_instance, window, nullptr, &surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface for secondary window!");
	}

	const VkQueueFamilyIndices indices = find_queue_families(m_physical_device);
	VkBool32 present_supported = VK_FALSE;
	vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, indices.m_present_family.value(), surface,
										 &present_supported);
	if (present_supported != VK_TRUE) {
		AQUILA_LOG_ERROR("VulkanDevice: present queue family does not support the secondary window surface");
	}

	return surface;
}

void VulkanDevice::destroy_surface_handle(VkSurfaceKHR surface) const {
	if (surface != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(m_vulkan_instance, surface, nullptr);
	}
}

Unique<IRHIQueryPool> VulkanDevice::create_timestamp_pool(Uint32 count) {
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(m_physical_device, &properties);
	if (properties.limits.timestampComputeAndGraphics == VK_FALSE || count == 0) {
		return nullptr;
	}
	auto pool = std::make_unique<VulkanQueryPool>(m_device, count, static_cast<F64>(properties.limits.timestampPeriod));
	if (pool->get_count() == 0) {
		return nullptr;
	}
	return pool;
}

Unique<IRHIRenderPass> VulkanDevice::create_render_pass(const RHI::RenderPassDesc &desc) {
	return std::make_unique<VulkanRenderPass>(*this, desc);
}

void VulkanDevice::submit(IRHICommandList &cmd) {
	auto &vk_cmd = static_cast<VulkanCommandList &>(cmd);
	VkCommandBuffer cmd_buf = vk_cmd.get_handle();

	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd_buf;

	submit_to_graphics_queue(&submit_info, VK_NULL_HANDLE);
}

void VulkanDevice::submit_and_wait(IRHICommandList &cmd) {
	auto &vk_cmd = static_cast<VulkanCommandList &>(cmd);
	VkCommandBuffer cmd_buf = vk_cmd.get_handle();

	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd_buf;

	VkFence fence = create_fence(false);
	switch (cmd.get_type()) {
	case CommandListType::Compute:
		submit_to_compute_queue(&submit_info, fence);
		break;
	case CommandListType::Transfer:
		submit_to_transfer_queue(&submit_info, fence);
		break;
	default:
		submit_to_graphics_queue(&submit_info, fence);
		break;
	}
	wait_for_fence(fence);
	destroy_fence(fence);
}

void VulkanDevice::present_frame(IRHISwapchain &swapchain, Uint32 image_index, Vec4 clear_color) {
	auto &vk_swapchain = static_cast<VulkanSwapchain &>(swapchain);
	const Uint32 slot = vk_swapchain.get_current_frame_slot();
	VkSemaphore image_available = vk_swapchain.get_image_available_semaphore(slot);
	VkSemaphore render_finished = vk_swapchain.get_render_finished_semaphore(image_index);
	VkFence slot_fence = vk_swapchain.get_in_flight_fence(slot);
	VkImage image = vk_swapchain.get_image(image_index);

	VkCommandPool pool = get_or_create_thread_local_graphics_pool();
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = pool;
	alloc_info.commandBufferCount = 1;
	AQUILA_VULKAN_CHECK(vkAllocateCommandBuffers(m_device, &alloc_info, &cmd));

	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	AQUILA_VULKAN_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

	VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	VkImageMemoryBarrier to_clear{};
	to_clear.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	to_clear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	to_clear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	to_clear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	to_clear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	to_clear.image = image;
	to_clear.subresourceRange = range;
	to_clear.srcAccessMask = 0;
	to_clear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
						 1, &to_clear);

	VkClearColorValue vk_clear{ .float32 = { clear_color.r, clear_color.g, clear_color.b, clear_color.a } };
	vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &vk_clear, 1, &range);

	VkImageMemoryBarrier to_present{};
	to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	to_present.image = image;
	to_present.subresourceRange = range;
	to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	to_present.dstAccessMask = 0;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
						 nullptr, 1, &to_present);

	AQUILA_VULKAN_CHECK(vkEndCommandBuffer(cmd));

	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &image_available;
	submit_info.pWaitDstStageMask = &wait_stage;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &render_finished;

	AQUILA_VULKAN_CHECK(vkResetFences(m_device, 1, &slot_fence));
	submit_to_graphics_queue(&submit_info, slot_fence);
	vk_swapchain.notify_frame_submitted(slot);
	vk_swapchain.defer_cmd_buf_free(slot, cmd, pool);

	vk_swapchain.present_image(image_index, render_finished);
}

bool VulkanDevice::is_frame_managed_pool(VkCommandPool pool) const {
	for (const auto &slot : m_frame_slots) {
		if (pool == slot.pool) {
			return true;
		}
	}
	return false;
}

void VulkanDevice::submit_swapchain_frame(VulkanCommandList &cmd, VulkanSwapchain &swapchain, Uint32 image_index) {
	const Uint32 slot = swapchain.get_current_frame_slot();
	VkCommandBuffer cmd_buf = cmd.get_handle();
	VkSemaphore image_available = swapchain.get_image_available_semaphore(slot);
	VkSemaphore render_finished = swapchain.get_render_finished_semaphore(image_index);
	VkFence slot_fence = swapchain.get_in_flight_fence(slot);

	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &image_available;
	submit_info.pWaitDstStageMask = &wait_stage;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd_buf;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &render_finished;

	{
		PROFILE_SCOPE("QueueSubmit");
		AQUILA_VULKAN_CHECK(vkResetFences(m_device, 1, &slot_fence));
		submit_to_graphics_queue(&submit_info, slot_fence);
		swapchain.notify_frame_submitted(slot);
	}

	if (!is_frame_managed_pool(cmd.get_pool())) {
		swapchain.defer_cmd_buf_free(slot, cmd_buf, cmd.get_pool());
	}

	{
		PROFILE_SCOPE("QueuePresent");
		swapchain.present_image(image_index, render_finished);
	}
}

void VulkanDevice::submit_offscreen_frame(VulkanCommandList &cmd) {
	const Uint32 slot = m_offscreen_frame_index;
	VkCommandBuffer cmd_buf = cmd.get_handle();

	AQUILA_VULKAN_CHECK(vkWaitForFences(m_device, 1, &m_offscreen_fences[slot], VK_TRUE, UINT64_MAX));

	for (auto &pending : m_offscreen_pending_cmd_bufs[slot]) {
		vkFreeCommandBuffers(m_device, pending.pool, 1, &pending.cmd);
	}
	m_offscreen_pending_cmd_bufs[slot].clear();
	m_deletion_queue->flush(slot);

	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd_buf;

	AQUILA_VULKAN_CHECK(vkResetFences(m_device, 1, &m_offscreen_fences[slot]));
	submit_to_graphics_queue(&submit_info, m_offscreen_fences[slot]);
	m_offscreen_pending_cmd_bufs[slot].push_back({ cmd_buf, cmd.get_pool() });

	m_offscreen_frame_index = (slot + 1) % SharedConstants::MAX_FRAMES_IN_FLIGHT;
	m_deletion_queue->set_current_slot(m_offscreen_frame_index);
}

void VulkanDevice::submit_frame(IRHICommandList &cmd, IRHISwapchain *swapchain, Uint32 image_index) {
	auto &vk_cmd = static_cast<VulkanCommandList &>(cmd);

	{
		PROFILE_SCOPE("EndCommandBuffer");
		cmd.end();
	}

	if (swapchain != nullptr) {
		submit_swapchain_frame(vk_cmd, static_cast<VulkanSwapchain &>(*swapchain), image_index);
	} else {
		submit_offscreen_frame(vk_cmd);
	}
}

RHI::DeletionQueue &VulkanDevice::get_deletion_queue() const {
	return *m_deletion_queue;
}

void VulkanDevice::copy_buffer(IRHICommandList &cmd, IRHIBuffer &src, IRHIBuffer &dst, Uint64 size, Uint64 src_offset,
							   Uint64 dst_offset) {
	auto &vk_cmd = static_cast<VulkanCommandList &>(cmd);
	auto &vk_src = static_cast<VulkanBuffer &>(src);
	auto &vk_dst = static_cast<VulkanBuffer &>(dst);

	VkBufferCopy region{};
	region.srcOffset = src_offset;
	region.dstOffset = dst_offset;
	region.size = size;
	vkCmdCopyBuffer(vk_cmd.get_handle(), vk_src.get_buffer(), vk_dst.get_buffer(), 1, &region);
}

Unique<IRHIPipeline> VulkanDevice::create_graphics_pipeline(const GraphicsPipelineDesc &desc) {
	std::vector<VkDescriptorSetLayout> vk_layouts;
	vk_layouts.reserve(desc.set_layouts.size());
	for (auto *layout : desc.set_layouts) {
		vk_layouts.push_back(static_cast<VulkanDescriptorSetLayout *>(layout)->get_descriptor_set_layout());
	}

	std::vector<VkPushConstantRange> push_ranges;
	push_ranges.reserve(desc.push_constants.size());
	for (const auto &pc : desc.push_constants) {
		push_ranges.push_back({ to_vk_shader_stage(pc.stages), pc.offset, pc.size });
	}

	VkPipelineLayoutCreateInfo layout_info{};
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.setLayoutCount = static_cast<Uint32>(vk_layouts.size());
	layout_info.pSetLayouts = vk_layouts.data();
	layout_info.pushConstantRangeCount = static_cast<Uint32>(push_ranges.size());
	layout_info.pPushConstantRanges = push_ranges.data();

	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	AQUILA_VULKAN_CHECK(vkCreatePipelineLayout(m_device, &layout_info, nullptr, &pipeline_layout));

	// Build shader stages (fragment is optional so omit for depth-only pipelines)
	auto make_module = [&](const ShaderStageDesc &s) {
		return VulkanShader::create_shader_module(s.spirv, *this, s.entry_point);
	};

	VkShaderModule vert_module = make_module(desc.vertex_shader);
	VkShaderModule frag_module =
		desc.fragment_shader.spirv.empty() ? VK_NULL_HANDLE : make_module(desc.fragment_shader);

	std::vector<VkPipelineShaderStageCreateInfo> stages;
	stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT,
					   vert_module, desc.vertex_shader.entry_point.c_str(), nullptr });
	if (frag_module != VK_NULL_HANDLE) {
		stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
						   VK_SHADER_STAGE_FRAGMENT_BIT, frag_module, desc.fragment_shader.entry_point.c_str(),
						   nullptr });
	}

	VulkanPipelineConfig config{};
	VulkanPipeline::default_pipeline_config(config);

	config.pipeline_layout = pipeline_layout;

	config.multisample_info.rasterizationSamples = to_vk_sample_count(desc.sample_count);
	config.multisample_info.sampleShadingEnable = desc.min_sample_shading ? VK_TRUE : VK_FALSE;
	config.multisample_info.minSampleShading = desc.min_sample_shading ? 1.0F : 0.0F;

	config.input_assembly_info.topology = to_vk_primitive_topology(desc.topology);

	config.rasterization_info.cullMode = to_vk_cull_mode(desc.raster.cull_mode);
	config.rasterization_info.polygonMode = to_vk_polygon_mode(desc.raster.fill_mode);
	config.rasterization_info.frontFace = to_vk_front_face(desc.raster.front_face);
	config.rasterization_info.depthClampEnable = desc.raster.depth_clamp ? VK_TRUE : VK_FALSE;
	config.rasterization_info.lineWidth = desc.raster.line_width;

	config.depth_stencil_info.depthTestEnable = desc.depth_stencil.depth_test ? VK_TRUE : VK_FALSE;
	config.depth_stencil_info.depthWriteEnable = desc.depth_stencil.depth_write ? VK_TRUE : VK_FALSE;
	config.depth_stencil_info.depthCompareOp = to_vk_compare_op(desc.depth_stencil.depth_compare);
	config.depth_stencil_info.stencilTestEnable = desc.depth_stencil.stencil_test ? VK_TRUE : VK_FALSE;

	config.color_blend_attachments.clear();
	for (const auto &att : desc.blend_attachments) {
		VkPipelineColorBlendAttachmentState vk_att{};
		vk_att.blendEnable = att.enable ? VK_TRUE : VK_FALSE;
		vk_att.srcColorBlendFactor = to_vk_blend_factor(att.src_color);
		vk_att.dstColorBlendFactor = to_vk_blend_factor(att.dst_color);
		vk_att.colorBlendOp = to_vk_blend_op(att.color_op);
		vk_att.srcAlphaBlendFactor = to_vk_blend_factor(att.src_alpha);
		vk_att.dstAlphaBlendFactor = to_vk_blend_factor(att.dst_alpha);
		vk_att.alphaBlendOp = to_vk_blend_op(att.alpha_op);
		vk_att.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		config.color_blend_attachments.push_back(vk_att);
	}
	config.color_blend_info.attachmentCount = static_cast<Uint32>(config.color_blend_attachments.size());
	config.color_blend_info.pAttachments = config.color_blend_attachments.data();

	for (auto fmt : desc.color_formats) {
		config.color_formats.push_back(to_vk_format(fmt));
	}
	if (desc.depth_format != TextureFormat::None) {
		config.depth_format = to_vk_format(desc.depth_format);
	}

	// Depth-only pipelines have no color attachments so blend state must match.
	if (config.color_formats.empty()) {
		config.color_blend_attachments.clear();
	}

	if (desc.no_vertex_input) {
		config.binding_descriptions.clear();
		config.attribute_descriptions.clear();
	} else if (desc.custom_vertex_layout.has_value()) {
		auto &layout = desc.custom_vertex_layout.value();
		config.binding_descriptions = { { 0, layout.stride, VK_VERTEX_INPUT_RATE_VERTEX } };
		config.attribute_descriptions.clear();
		for (auto &a : layout.attributes) {
			config.attribute_descriptions.push_back({ a.location, a.binding, to_vk_format(a.format), a.offset });
		}
	} else {
		config.binding_descriptions = Vertex::get_binding_descriptions();
		config.attribute_descriptions = Vertex::get_attribute_descriptions();
	}
	auto pipeline = std::make_unique<VulkanPipeline>(*this, stages, config);

	vkDestroyShaderModule(m_device, vert_module, nullptr);
	if (frag_module != VK_NULL_HANDLE) {
		vkDestroyShaderModule(m_device, frag_module, nullptr);
	}

	return pipeline;
}

Unique<IRHIPipeline> VulkanDevice::create_compute_pipeline(const ComputePipelineDesc &desc) {
	std::vector<VkDescriptorSetLayout> vk_layouts;
	vk_layouts.reserve(desc.set_layouts.size());
	for (auto *layout : desc.set_layouts) {
		vk_layouts.push_back(static_cast<VulkanDescriptorSetLayout *>(layout)->get_descriptor_set_layout());
	}

	std::vector<VkPushConstantRange> push_ranges;
	push_ranges.reserve(desc.push_constants.size());
	for (const auto &pc : desc.push_constants) {
		push_ranges.push_back({ to_vk_shader_stage(pc.stages), pc.offset, pc.size });
	}

	VkPipelineLayoutCreateInfo layout_info{};
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.setLayoutCount = static_cast<Uint32>(vk_layouts.size());
	layout_info.pSetLayouts = vk_layouts.data();
	layout_info.pushConstantRangeCount = static_cast<Uint32>(push_ranges.size());
	layout_info.pPushConstantRanges = push_ranges.data();

	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	AQUILA_VULKAN_CHECK(vkCreatePipelineLayout(m_device, &layout_info, nullptr, &pipeline_layout));

	VkShaderModule comp_module =
		VulkanShader::create_shader_module(desc.compute_shader.spirv, *this, desc.compute_shader.entry_point);

	auto pipeline =
		std::make_unique<VulkanComputePipeline>(*this, comp_module, desc.compute_shader.entry_point, pipeline_layout);

	// Layout is owned by VulkanComputePipeline — do not destroy here.
	vkDestroyShaderModule(m_device, comp_module, nullptr);

	return pipeline;
}

Unique<IRHIDescriptorSetLayout> VulkanDevice::create_descriptor_set_layout(const DescriptorSetLayoutDesc &desc) {
	VulkanDescriptorSetLayout::Builder builder(*this);
	for (const auto &b : desc.bindings) {
		builder.add_binding(b.binding, to_vk_descriptor_type(b.type), to_vk_shader_stage(b.stages), b.count);
	}
	return builder.build();
}

Unique<IRHIDescriptorSet> VulkanDevice::allocate_descriptor_set(IRHIDescriptorSetLayout &layout) {
	auto &vk_layout = static_cast<VulkanDescriptorSetLayout &>(layout);
	VkDescriptorSet set = VK_NULL_HANDLE;
	if (!m_global_pool->allocate_descriptor(vk_layout.get_descriptor_set_layout(), set)) {
		throw std::runtime_error("Global descriptor pool exhausted");
	}
	return Unique<IRHIDescriptorSet>(new VulkanDescriptorSet(*this, set, vk_layout, *m_global_pool));
}

void VulkanDevice::create_global_descriptor_pool() {
	std::vector<VkDescriptorPoolSize> pool_sizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },		 { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 }, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 256 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 64 },
	};
	m_global_pool = std::make_unique<VulkanDescriptorPool>(
		*this, 4000, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, pool_sizes);
}

void VulkanDevice::destroy_global_descriptor_pool() {
	m_global_pool.reset();
}

void VulkanDevice::create_graphics_command_pool() {
	const VkQueueFamilyIndices queue_family_indices = find_queue_families(m_physical_device);

	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = queue_family_indices.m_graphics_family.value();
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	AQUILA_VULKAN_CHECK(vkCreateCommandPool(m_device, &pool_info, nullptr, &m_graphics_command_pool));
}

void VulkanDevice::create_compute_command_pool() {
	const VkQueueFamilyIndices queue_families_indices = find_queue_families(m_physical_device);

	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = queue_families_indices.m_compute_family.value();
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	AQUILA_VULKAN_CHECK(vkCreateCommandPool(m_device, &pool_info, nullptr, &m_compute_command_pool));
}

void VulkanDevice::create_transfer_command_pool() {
	const VkQueueFamilyIndices indices = find_queue_families(m_physical_device);

	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = indices.m_transfer_family.value();
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	AQUILA_VULKAN_CHECK(vkCreateCommandPool(m_device, &pool_info, nullptr, &m_transfer_command_pool));
}

void VulkanDevice::create_frame_command_pools() {
	const VkQueueFamilyIndices indices = find_queue_families(m_physical_device);

	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = indices.m_graphics_family.value();
	pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;

	for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
		AQUILA_VULKAN_CHECK(vkCreateCommandPool(m_device, &pool_info, nullptr, &m_frame_slots[i].pool));
		alloc_info.commandPool = m_frame_slots[i].pool;
		AQUILA_VULKAN_CHECK(vkAllocateCommandBuffers(m_device, &alloc_info, &m_frame_slots[i].cmd));

		std::string name = "FrameCmd_" + std::to_string(i);
		set_object_debug_name(VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<Uint64>(m_frame_slots[i].cmd),
							  name.c_str());
	}
}

void VulkanDevice::reset_frame_command_pool(Uint32 slot) {
	AQUILA_ASSERT(slot < SharedConstants::MAX_FRAMES_IN_FLIGHT, "Frame slot out of range");
	vkResetCommandPool(m_device, m_frame_slots[slot].pool, 0);
}

Unique<IRHICommandList> VulkanDevice::create_frame_command_list(Uint32 slot) {
	AQUILA_ASSERT(slot < SharedConstants::MAX_FRAMES_IN_FLIGHT, "Frame slot out of range");
	return std::make_unique<VulkanCommandList>(*this, m_frame_slots[slot].pool, m_frame_slots[slot].cmd,
											   CommandListType::Graphics, "FrameCmd_" + std::to_string(slot));
}

void VulkanDevice::submit_to_compute_queue(const VkSubmitInfo *submit_info, VkFence fence) {
	std::lock_guard<std::mutex> lock(m_compute_queue_mutex);
	AQUILA_VULKAN_CHECK(vkQueueSubmit(m_compute_queue, 1, submit_info, fence));
}

void VulkanDevice::submit_to_graphics_queue(const VkSubmitInfo *submit_info, VkFence fence) {
	std::lock_guard<std::mutex> lock(m_graphics_queue_mutex);
	AQUILA_VULKAN_CHECK(vkQueueSubmit(m_graphics_queue, 1, submit_info, fence));
}

void VulkanDevice::submit_to_transfer_queue(const VkSubmitInfo *submit_info, VkFence fence) {
	std::lock_guard<std::mutex> lock(m_transfer_queue_mutex);
	AQUILA_VULKAN_CHECK(vkQueueSubmit(m_transfer_queue, 1, submit_info, fence));
}

VkResult VulkanDevice::present_to_queue(const VkPresentInfoKHR *present_info) {
	std::mutex &queue_mutex = (m_present_queue == m_graphics_queue) ? m_graphics_queue_mutex : m_present_queue_mutex;
	std::lock_guard<std::mutex> lock(queue_mutex);
	return vkQueuePresentKHR(m_present_queue, present_info);
}

void VulkanDevice::wait_graphics_queue_idle() {
	std::lock_guard<std::mutex> lock(m_graphics_queue_mutex);
	AQUILA_VULKAN_CHECK(vkQueueWaitIdle(m_graphics_queue));
}

void VulkanDevice::wait_transfer_queue_idle() {
	std::lock_guard<std::mutex> lock(m_transfer_queue_mutex);
	AQUILA_VULKAN_CHECK(vkQueueWaitIdle(m_transfer_queue));
}

VkSampler VulkanDevice::get_or_create_sampler(const SamplerDesc &desc) {
	auto it = m_sampler_cache.find(desc);
	if (it != m_sampler_cache.end()) {
		return it->second;
	}

	VkPhysicalDeviceProperties props{};
	vkGetPhysicalDeviceProperties(m_physical_device, &props);

	VkSamplerCreateInfo info = to_vk_sampler_create_info(desc, props.limits.maxSamplerAnisotropy);

	VkSampler sampler = nullptr;
	AQUILA_VULKAN_CHECK(vkCreateSampler(m_device, &info, nullptr, &sampler));
	m_sampler_cache[desc] = sampler;
	return sampler;
}

void VulkanDevice::destroy_sampler_cache() {
	for (auto &[desc, sampler] : m_sampler_cache) {
		vkDestroySampler(m_device, sampler, nullptr);
	}
	m_sampler_cache.clear();
}

VkCommandPool VulkanDevice::get_or_create_thread_local_graphics_pool() {
	auto queue_families_indices = find_queue_families(m_physical_device);
	auto id = std::this_thread::get_id();

	{
		std::lock_guard<std::mutex> lock(m_thread_pool_map_mutex);
		auto it = m_thread_pools.find(id);
		if (it != m_thread_pools.end()) {
			return it->second.pool;
		}
	}

	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = queue_families_indices.m_graphics_family.value();
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandPool pool = nullptr;
	vkCreateCommandPool(m_device, &pool_info, nullptr, &pool);

	std::lock_guard<std::mutex> lock(m_thread_pool_map_mutex);
	m_thread_pools[id].pool = pool;
	return pool;
}

VkFence VulkanDevice::create_fence(bool signaled) {
	VkFenceCreateInfo fence_info{};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

	VkFence fence = nullptr;
	AQUILA_VULKAN_CHECK(vkCreateFence(m_device, &fence_info, nullptr, &fence));
	return fence;
}

void VulkanDevice::wait_for_fence(VkFence fence) {
	AQUILA_VULKAN_CHECK(vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX));
}

void VulkanDevice::destroy_fence(VkFence fence) {
	vkDestroyFence(m_device, fence, nullptr);
}

VkFormat VulkanDevice::find_supported_format(const std::vector<VkFormat> &candidates, VkImageTiling tiling,
											 VkFormatFeatureFlags features) {
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(m_physical_device, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		}
		if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}
	throw std::runtime_error("failed to find supported format!");
}

void VulkanDevice::set_object_debug_name(VkObjectType object_type, Uint64 handle, const char *name) const {
	if ((m_vk_set_debug_utils_object_name_ext == nullptr) || (name == nullptr)) {
		return;
	}
	VkDebugUtilsObjectNameInfoEXT name_info{};
	name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	name_info.objectType = object_type;
	name_info.objectHandle = handle;
	name_info.pObjectName = name;
	m_vk_set_debug_utils_object_name_ext(m_device, &name_info);
}

void VulkanDevice::log_device_info() const {
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(m_physical_device, &properties);

	AQUILA_LOG_INFO("Graphics Device Information");
	AQUILA_LOG_INFO("Device Name: {}", properties.deviceName);

	std::string device_type;
	switch (properties.deviceType) {
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		device_type = "Discrete GPU";
		break;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		device_type = "Integrated GPU";
		break;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
		device_type = "Virtual GPU";
		break;
	case VK_PHYSICAL_DEVICE_TYPE_CPU:
		device_type = "CPU";
		break;
	default:
		device_type = "Unknown";
	}

	AQUILA_LOG_INFO("Device Type: " + device_type);

	const Uint32 driver_version = properties.driverVersion;
	const Uint32 api_version = properties.apiVersion;

	AQUILA_LOG_INFO("Driver Version: " + std::to_string(VK_VERSION_MAJOR(driver_version)) + "." +
					std::to_string(VK_VERSION_MINOR(driver_version)) + "." +
					std::to_string(VK_VERSION_PATCH(driver_version)));

	AQUILA_LOG_INFO("Vulkan Version: " + std::to_string(VK_VERSION_MAJOR(api_version)) + "." +
					std::to_string(VK_VERSION_MINOR(api_version)) + "." +
					std::to_string(VK_VERSION_PATCH(api_version)));

	VkPhysicalDeviceMemoryProperties mem_properties;
	vkGetPhysicalDeviceMemoryProperties(m_physical_device, &mem_properties);

	Uint64 total_memory = 0;
	for (Uint32 i = 0; i < mem_properties.memoryHeapCount; i++) {
		if ((mem_properties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0U) {
			total_memory += mem_properties.memoryHeaps[i].size;
		}
	}
	AQUILA_LOG_INFO("Device Memory: {}", std::to_string(total_memory / (1024 * 1024)) + " MB");

	VkQueueFamilyIndices indices = find_queue_families(m_physical_device);
	AQUILA_LOG_INFO("Graphics Queue Family: " + std::to_string(indices.m_graphics_family.value()));
	AQUILA_LOG_INFO("Present Queue Family: " + std::to_string(indices.m_present_family.value()));
	AQUILA_LOG_INFO("Compute Queue Family: " + std::to_string(indices.m_compute_family.value()));
	AQUILA_LOG_INFO("Transfer Queue Family: " + std::to_string(indices.m_transfer_family.value()));
}

void VulkanDevice::create_instance() {
	if (enableValidationLayers && !check_validation_layer_support()) {
		throw std::runtime_error("validation layers requested, but not available!");
	}

	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Aquila Engine";
	app_info.applicationVersion =
		VK_MAKE_API_VERSION(0, AQUILA_VERSION_MAJOR, AQUILA_VERSION_MINOR, AQUILA_VERSION_PATCH);
	app_info.pEngineName = "Aquila";
	app_info.engineVersion = VK_MAKE_API_VERSION(0, AQUILA_VERSION_MAJOR, AQUILA_VERSION_MINOR, AQUILA_VERSION_PATCH);
	app_info.apiVersion = VK_API_VERSION_1_4;

	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;

	const auto extensions = get_required_extensions();
	create_info.enabledExtensionCount = static_cast<Uint32>(extensions.size());
	create_info.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
	if (enableValidationLayers) {
		create_info.enabledLayerCount = static_cast<Uint32>(VALIDATION_LAYERS.size());
		create_info.ppEnabledLayerNames = VALIDATION_LAYERS.data();
		populate_debug_messenger_create_info(debug_create_info);
		create_info.pNext = &debug_create_info;
	} else {
		create_info.enabledLayerCount = 0;
		create_info.pNext = nullptr;
	}

	AQUILA_VULKAN_CHECK(vkCreateInstance(&create_info, nullptr, &m_vulkan_instance));
}

void VulkanDevice::create_surface() {
	if (glfwCreateWindowSurface(m_vulkan_instance, &m_window_handle, nullptr, &m_surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface!");
	}
}

void VulkanDevice::initialize_vma() {
	VmaVulkanFunctions vulkan_functions = {};
	vulkan_functions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
	vulkan_functions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo allocator_create_info = {};
	allocator_create_info.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
	allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_4;
	allocator_create_info.physicalDevice = m_physical_device;
	allocator_create_info.device = m_device;
	allocator_create_info.instance = m_vulkan_instance;
	allocator_create_info.pVulkanFunctions = &vulkan_functions;

	vmaCreateAllocator(&allocator_create_info, &m_allocator);
}

bool VulkanDevice::check_validation_layer_support() const {
	Uint32 layer_count = 0;
	vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

	std::vector<VkLayerProperties> available_layers(layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

	for (const char *layer_name : VALIDATION_LAYERS) {
		bool layer_found = false;
		for (const auto &layer_properties : available_layers) {
			if (strcmp(layer_name, layer_properties.layerName) == 0) {
				layer_found = true;
				break;
			}
		}
		if (!layer_found) {
			return false;
		}
	}
	return true;
}

std::vector<const char *> VulkanDevice::get_required_extensions() const {
	Uint32 glfw_extension_count = 0;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

	std::vector<const char *> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

	if (enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

void VulkanDevice::create_logical_device() {
	VkQueueFamilyIndices indices = find_queue_families(m_physical_device);

	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
	std::set<Uint32> unique_queue_families = { indices.m_graphics_family.value(), indices.m_present_family.value(),
											   indices.m_compute_family.value(), indices.m_transfer_family.value() };

	std::vector<F32> queue_priorities = { 1.0F, 0.5F };

	for (Uint32 queue_family : unique_queue_families) {
		VkDeviceQueueCreateInfo queue_create_info{};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = queue_family;

		if (queue_family == indices.m_graphics_family.value()) {
			uint32_t queue_family_count = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, nullptr);
			std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
			vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, queue_families.data());
			Uint32 available_queues = queue_families[queue_family].queueCount;
			queue_create_info.queueCount = std::min(2U, available_queues);
			queue_create_info.pQueuePriorities = queue_priorities.data();
		} else {
			queue_create_info.queueCount = 1;
			queue_create_info.pQueuePriorities = queue_priorities.data();
		}

		queue_create_infos.push_back(queue_create_info);
	}

	VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features{};
	dynamic_rendering_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
	dynamic_rendering_features.dynamicRendering = VK_TRUE;

	VkPhysicalDeviceFeatures device_features{};
	device_features.samplerAnisotropy = VK_TRUE;
	device_features.wideLines = VK_TRUE;
	device_features.largePoints = VK_TRUE;
	device_features.fillModeNonSolid = VK_TRUE;
	device_features.independentBlend = VK_TRUE;
	device_features.sampleRateShading = VK_TRUE;

  VkPhysicalDeviceVulkan11Features vulkan11_features{};
  vulkan11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  vulkan11_features.shaderDrawParameters = VK_TRUE;
  vulkan11_features.pNext = &dynamic_rendering_features;

	VkPhysicalDeviceFeatures2 device_features2{};
	device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	device_features2.pNext = &vulkan11_features;
	device_features2.features = device_features;

	VkDeviceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.queueCreateInfoCount = static_cast<Uint32>(queue_create_infos.size());
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.pNext = &device_features2;
	create_info.pEnabledFeatures = nullptr;
	create_info.enabledExtensionCount = static_cast<Uint32>(DEVICE_EXTENSIONS.size());
	create_info.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();

	if (vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device) != VK_SUCCESS) {
		throw std::runtime_error("failed to create logical device!");
	}

	bool compute_shares_graphics = indices.m_compute_family.value() == indices.m_graphics_family.value();

	vkGetDeviceQueue(m_device, indices.m_graphics_family.value(), 0, &m_graphics_queue);
	vkGetDeviceQueue(m_device, indices.m_present_family.value(), 0, &m_present_queue);
	vkGetDeviceQueue(m_device, indices.m_transfer_family.value(), 0, &m_transfer_queue);

	if (compute_shares_graphics) {
		Uint32 queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, nullptr);
		std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, queue_families.data());

		Uint32 available_queues = queue_families[indices.m_graphics_family.value()].queueCount;
		Uint32 compute_queue_index = (available_queues > 1) ? 1 : 0;
		vkGetDeviceQueue(m_device, indices.m_compute_family.value(), compute_queue_index, &m_compute_queue);
	} else {
		vkGetDeviceQueue(m_device, indices.m_compute_family.value(), 0, &m_compute_queue);
	}
}

void VulkanDevice::pick_physical_device() {
	Uint32 device_count = 0;
	vkEnumeratePhysicalDevices(m_vulkan_instance, &device_count, nullptr);

	if (device_count == 0) {
		AQUILA_LOG_CRITICAL("Failed to find GPUs with Vulkan support!");
		abort();
	}

	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(m_vulkan_instance, &device_count, devices.data());

	for (const auto &device : devices) {
		if (is_suitable(device)) {
			m_physical_device = device;
			break;
		}
	}

	if (m_physical_device == VK_NULL_HANDLE) {
		AQUILA_LOG_CRITICAL("Failed to find a suitable GPU!");
		abort();
	}
}

bool VulkanDevice::is_suitable(const VkPhysicalDevice vk_physical_device) {
	VkQueueFamilyIndices indices = find_queue_families(vk_physical_device);
	const bool extension_supported = check_device_extension_support(vk_physical_device);

	bool swap_chain_adequate = false;
	if (extension_supported) {
		const VkSwapChainSupportDetails swap_chain_support = query_swap_chain_support(vk_physical_device, m_surface);
		swap_chain_adequate = !swap_chain_support.m_formats.empty() && !swap_chain_support.m_present_modes.empty();
	}

	return indices.is_complete() && extension_supported && swap_chain_adequate;
}

bool VulkanDevice::check_device_extension_support(const VkPhysicalDevice vk_physical_device) const {
	Uint32 extension_count = 0;
	vkEnumerateDeviceExtensionProperties(vk_physical_device, nullptr, &extension_count, nullptr);

	std::vector<VkExtensionProperties> available_extensions(extension_count);
	vkEnumerateDeviceExtensionProperties(vk_physical_device, nullptr, &extension_count, available_extensions.data());

	std::set<std::string> required_extensions(DEVICE_EXTENSIONS.begin(), DEVICE_EXTENSIONS.end());
	for (const auto &extension : available_extensions) {
		required_extensions.erase(extension.extensionName);
	}

	return required_extensions.empty();
}

VkQueueFamilyIndices VulkanDevice::find_queue_families(const VkPhysicalDevice vk_physical_device) const {
	VkQueueFamilyIndices indices;

	Uint32 queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device, &queue_family_count, nullptr);

	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device, &queue_family_count, queue_families.data());

	int index = 0;
	for (const auto &queue_family : queue_families) {
		if ((queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
			indices.m_graphics_family = index;
		}

		if (((queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0U) &&
			((queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0U) &&
			((queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0U)) {
			indices.m_transfer_family = index;
		}

		if ((queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0U) {
			bool is_dedicated = (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0U;
			VkBool32 present_support = 0U;
			vkGetPhysicalDeviceSurfaceSupportKHR(vk_physical_device, index, m_surface, &present_support);
			bool is_present = present_support != 0U;

			if (!is_present) {
				if (is_dedicated || !indices.m_compute_family.has_value()) {
					indices.m_compute_family = index;
				}
			}
		}

		VkBool32 present_support = 0U;
		vkGetPhysicalDeviceSurfaceSupportKHR(vk_physical_device, index, m_surface, &present_support);
		if (present_support != 0U) {
			indices.m_present_family = index;
		}

		if (indices.is_complete()) {
			break;
		}

		index++;
	}

	if (!indices.m_compute_family.has_value() && indices.m_graphics_family.has_value()) {
		indices.m_compute_family = indices.m_graphics_family;
	}
	if (!indices.m_transfer_family.has_value()) {
		indices.m_transfer_family = indices.m_compute_family;
	}

	return indices;
}

VkSwapChainSupportDetails VulkanDevice::query_swap_chain_support(VkPhysicalDevice vk_physical_device,
																 VkSurfaceKHR surface) const {
	VkSwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physical_device, surface, &details.m_surface_capabilities);

	Uint32 format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, surface, &format_count, nullptr);
	if (format_count != 0) {
		details.m_formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, surface, &format_count, details.m_formats.data());
	}

	Uint32 present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(vk_physical_device, surface, &present_mode_count, nullptr);
	if (present_mode_count != 0) {
		details.m_present_modes.resize(present_mode_count);
		AQUILA_VULKAN_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(vk_physical_device, surface, &present_mode_count,
																	  details.m_present_modes.data()));
	}

	return details;
}

void VulkanDevice::setup_debug_messenger() {
	if (!enableValidationLayers) {
		return;
	}
	VkDebugUtilsMessengerCreateInfoEXT create_info;
	populate_debug_messenger_create_info(create_info);
	AQUILA_VULKAN_CHECK(create_debug_messenger_ext(m_vulkan_instance, &create_info, nullptr, &m_debug_messenger));

	m_vk_set_debug_utils_object_name_ext =
		(PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(m_device, "vkSetDebugUtilsObjectNameEXT");
	m_vk_cmd_begin_debug_utils_label_ext =
		(PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(m_device, "vkCmdBeginDebugUtilsLabelEXT");
	m_vk_cmd_end_debug_utils_label_ext =
		(PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(m_device, "vkCmdEndDebugUtilsLabelEXT");
}

VkResult VulkanDevice::create_debug_messenger_ext(const VkInstance instance,
												  const VkDebugUtilsMessengerCreateInfoEXT *p_create_info,
												  const VkAllocationCallbacks *p_allocator,
												  VkDebugUtilsMessengerEXT *p_debug_messenger) {
	if (const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
		func != nullptr) {
		return func(instance, p_create_info, p_allocator, p_debug_messenger);
	}
	return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void VulkanDevice::destroy_debug_messenger_ext(const VkInstance instance,
											   const VkDebugUtilsMessengerEXT debug_messenger,
											   const VkAllocationCallbacks *p_allocator) {
	const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
		vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
	if (func != nullptr) {
		func(instance, debug_messenger, p_allocator);
	}
}

void VulkanDevice::populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT &create_info) {
	create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	create_info.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	create_info.pfnUserCallback = debug_callback;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDevice::debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
															VkDebugUtilsMessageTypeFlagsEXT message_type,
															const VkDebugUtilsMessengerCallbackDataEXT *p_callback_data,
															void *p_user_data) {
	if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		AQUILA_LOG_ERROR("Vulkan: {}", p_callback_data->pMessage);
	} else if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		AQUILA_LOG_WARNING("Vulkan: {}", p_callback_data->pMessage);
	}
	return VK_FALSE;
}

} // namespace Aquila::RHI
