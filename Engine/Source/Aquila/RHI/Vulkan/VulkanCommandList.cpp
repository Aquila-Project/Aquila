#include "Aquila/RHI/Vulkan/VulkanCommandList.h"
#include "Aquila/RHI/Vulkan/VulkanQueryPool.h"
#include "Aquila/RHI/Vulkan/VulkanDevice.h"
#include "Aquila/RHI/Vulkan/VulkanTexture.h"
#include "Aquila/RHI/Vulkan/VulkanBuffer.h"
#include "Aquila/RHI/Vulkan/VulkanPipeline.h"
#include "Aquila/RHI/Vulkan/VulkanComputePipeline.h"
#include "Aquila/RHI/Vulkan/VulkanDescriptorSet.h"
#include "Aquila/RHI/Vulkan/VulkanFormatUtils.h"

namespace Aquila::RHI {

// ResourceState -> Vulkan barrier helpers

namespace {

struct VkTexBarrierInfo {
	VkPipelineStageFlags stage = 0;
	VkAccessFlags access = 0;
	VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct VkBufBarrierInfo {
	VkPipelineStageFlags stage = 0;
	VkAccessFlags access = 0;
};

static VkTexBarrierInfo tex_barrier_info(ResourceState state, bool is_depth) {
	VkTexBarrierInfo out{};

	if (state == ResourceState::Undefined) {
		out.stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		out.access = 0;
		out.layout = VK_IMAGE_LAYOUT_UNDEFINED;
		return out;
	}

	const auto s = static_cast<Uint16>(state);
	const auto has = [s](ResourceState bit) { return (s & static_cast<Uint16>(bit)) != 0; };

	if (has(ResourceState::ColorAttachmentWrite)) {
		out.stage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		out.access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		out.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
	if (has(ResourceState::ColorAttachmentRead)) {
		out.stage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		out.access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		if (out.layout == VK_IMAGE_LAYOUT_UNDEFINED) {
			out.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
	}

	// Depth write takes precedence over depth read for layout selection
	if (has(ResourceState::DepthStencilWrite)) {
		out.stage |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		out.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		out.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	} else if (has(ResourceState::DepthStencilRead)) {
		out.stage |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		out.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		if (out.layout == VK_IMAGE_LAYOUT_UNDEFINED) {
			out.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}
	}

	if (has(ResourceState::ShaderRead)) {
		out.stage |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		out.access |= VK_ACCESS_SHADER_READ_BIT;
		if (out.layout == VK_IMAGE_LAYOUT_UNDEFINED) {
			out.layout =
				is_depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	}

	// Storage overrides layout to GENERAL regardless of other flags
	if (has(ResourceState::StorageRead) || has(ResourceState::StorageWrite)) {
		out.stage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		if (has(ResourceState::StorageRead)) {
			out.access |= VK_ACCESS_SHADER_READ_BIT;
		}
		if (has(ResourceState::StorageWrite)) {
			out.access |= VK_ACCESS_SHADER_WRITE_BIT;
		}
		out.layout = VK_IMAGE_LAYOUT_GENERAL;
	}

	if (has(ResourceState::TransferSrc)) {
		out.stage |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		out.access |= VK_ACCESS_TRANSFER_READ_BIT;
		if (out.layout == VK_IMAGE_LAYOUT_UNDEFINED) {
			out.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		}
	}
	if (has(ResourceState::TransferDst)) {
		out.stage |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		out.access |= VK_ACCESS_TRANSFER_WRITE_BIT;
		if (out.layout == VK_IMAGE_LAYOUT_UNDEFINED) {
			out.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		}
	}

	if (has(ResourceState::Present)) {
		out.stage |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		out.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}

	if (out.stage == 0) {
		out.stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	}
	return out;
}

static VkBufBarrierInfo buf_barrier_info(ResourceState state) {
	VkBufBarrierInfo out{};

	if (state == ResourceState::Undefined) {
		out.stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		return out;
	}

	const auto s = static_cast<Uint16>(state);
	const auto has = [s](ResourceState bit) { return (s & static_cast<Uint16>(bit)) != 0; };

	if (has(ResourceState::UniformRead)) {
		out.stage |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		out.access |= VK_ACCESS_UNIFORM_READ_BIT;
	}
	if (has(ResourceState::ShaderRead)) {
		out.stage |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		out.access |= VK_ACCESS_SHADER_READ_BIT;
	}
	if (has(ResourceState::StorageRead)) {
		out.stage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		out.access |= VK_ACCESS_SHADER_READ_BIT;
	}
	if (has(ResourceState::StorageWrite)) {
		out.stage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		out.access |= VK_ACCESS_SHADER_WRITE_BIT;
	}
	if (has(ResourceState::TransferSrc)) {
		out.stage |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		out.access |= VK_ACCESS_TRANSFER_READ_BIT;
	}
	if (has(ResourceState::TransferDst)) {
		out.stage |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		out.access |= VK_ACCESS_TRANSFER_WRITE_BIT;
	}
	if (has(ResourceState::IndirectArgument)) {
		out.stage |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
		out.access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	}
	if (has(ResourceState::IndexBuffer)) {
		out.stage |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		out.access |= VK_ACCESS_INDEX_READ_BIT;
	}
	if (has(ResourceState::VertexBuffer)) {
		out.stage |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		out.access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	}
	if (has(ResourceState::HostRead)) {
		out.stage |= VK_PIPELINE_STAGE_HOST_BIT;
		out.access |= VK_ACCESS_HOST_READ_BIT;
	}

	if (out.stage == 0) {
		out.stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	}
	return out;
}

} // anonymous namespace

// Lifecycle

VulkanCommandList::VulkanCommandList(VulkanDevice &device, VkCommandPool command_pool, CommandListType type,
									 const std::string &name)
	: m_command_pool(command_pool), m_type(type), m_name(name), m_device(device) {
	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = command_pool;
	alloc_info.commandBufferCount = 1;

	AQUILA_VULKAN_CHECK(vkAllocateCommandBuffers(m_device.get_device(), &alloc_info, &m_command_buffer));

	m_device.set_object_debug_name(VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<Uint64>(m_command_buffer),
								   name.c_str());
}

VulkanCommandList::VulkanCommandList(VulkanDevice &device, VkCommandPool command_pool, VkCommandBuffer existing_cmd,
									 CommandListType type, const std::string &name)
	: m_command_buffer(existing_cmd), m_command_pool(command_pool), m_type(type), m_name(name), m_device(device) {
	m_device.set_object_debug_name(VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<Uint64>(m_command_buffer),
								   name.c_str());
}

VulkanCommandList::~VulkanCommandList() {
	// Freed automatically when the command pool is destroyed
}

void VulkanCommandList::begin() {
	if (m_is_recording) {
		throw std::runtime_error("Command list '" + m_name + "' is already recording");
	}

	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	AQUILA_VULKAN_CHECK(vkBeginCommandBuffer(m_command_buffer, &begin_info));
	m_is_recording = true;
}

void VulkanCommandList::end() {
	if (!m_is_recording) {
		throw std::runtime_error("Command list '" + m_name + "' is not recording");
	}

	AQUILA_VULKAN_CHECK(vkEndCommandBuffer(m_command_buffer));
	m_is_recording = false;
}

void VulkanCommandList::reset() {
	vkResetCommandBuffer(m_command_buffer, 0);
	m_is_recording = false;
	m_bound_pipeline_layout = VK_NULL_HANDLE;
}

// Resource transitions

void VulkanCommandList::transition_texture(IRHITexture &texture, ResourceState old_state, ResourceState new_state) {
	if (old_state == new_state) {
		return;
	}

	auto &vk_tex = static_cast<VulkanTexture &>(texture);
	const bool depth = is_depth_format(vk_tex.get_format());

	VkTexBarrierInfo src = tex_barrier_info(old_state, depth);
	VkTexBarrierInfo dst = tex_barrier_info(new_state, depth);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = src.access;
	barrier.dstAccessMask = dst.access;
	barrier.oldLayout = src.layout;
	barrier.newLayout = dst.layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = vk_tex.get_image();
	barrier.subresourceRange = { to_vk_image_aspect(vk_tex.get_format()), 0, VK_REMAINING_MIP_LEVELS, 0,
								 VK_REMAINING_ARRAY_LAYERS };

	vkCmdPipelineBarrier(m_command_buffer, src.stage, dst.stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VulkanCommandList::transition_buffer(IRHIBuffer &buffer, ResourceState old_state, ResourceState new_state) {
	if (old_state == new_state) {
		return;
	}

	auto &vk_buf = static_cast<VulkanBuffer &>(buffer);

	VkBufBarrierInfo src = buf_barrier_info(old_state);
	VkBufBarrierInfo dst = buf_barrier_info(new_state);

	VkBufferMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = src.access;
	barrier.dstAccessMask = dst.access;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = vk_buf.get_buffer();
	barrier.offset = 0;
	barrier.size = VK_WHOLE_SIZE;

	vkCmdPipelineBarrier(m_command_buffer, src.stage, dst.stage, 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

// Pipeline and state

void VulkanCommandList::bind_pipeline(IRHIPipeline &pipeline) {
	if (pipeline.get_bind_point() == PipelineBindPoint::Compute) {
		auto &vk_pipeline = static_cast<VulkanComputePipeline &>(pipeline);
		m_bound_pipeline_layout = vk_pipeline.get_layout();
		m_bound_bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
		vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk_pipeline.get_pipeline());
	} else {
		auto &vk_pipeline = static_cast<VulkanPipeline &>(pipeline);
		m_bound_pipeline_layout = vk_pipeline.get_layout();
		m_bound_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
		vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline.get_pipeline());
	}
}

void VulkanCommandList::set_viewport(float x, float y, float width, float height, float min_depth, float max_depth) {
	VkViewport vp{ x, y, width, height, min_depth, max_depth };
	vkCmdSetViewport(m_command_buffer, 0, 1, &vp);
}

void VulkanCommandList::set_scissor(Int32 x, Int32 y, Uint32 width, Uint32 height) {
	VkRect2D s{ { x, y }, { width, height } };
	vkCmdSetScissor(m_command_buffer, 0, 1, &s);
}

// Resource binding

void VulkanCommandList::bind_descriptor_set(Uint32 set, IRHIDescriptorSet &descriptor_set) {
	AQUILA_ASSERT(m_bound_pipeline_layout != VK_NULL_HANDLE, "BindDescriptorSet called before BindPipeline");
	auto &vk_set = static_cast<VulkanDescriptorSet &>(descriptor_set);
	VkDescriptorSet raw = vk_set.get_descriptor_set();
	vkCmdBindDescriptorSets(m_command_buffer, m_bound_bind_point, m_bound_pipeline_layout, set, 1, &raw, 0, nullptr);
}

void VulkanCommandList::push_constants(const void *data, Uint32 size, ShaderStageFlags stages, Uint32 offset) {
	AQUILA_ASSERT(m_bound_pipeline_layout != VK_NULL_HANDLE, "PushConstants called before BindPipeline");
	vkCmdPushConstants(m_command_buffer, m_bound_pipeline_layout, to_vk_shader_stage(stages), offset, size, data);
}

void VulkanCommandList::bind_vertex_buffer(IRHIBuffer &buffer, Uint32 binding, Uint64 offset) {
	auto &vk_buf = static_cast<VulkanBuffer &>(buffer);
	VkBuffer raw = vk_buf.get_buffer();
	VkDeviceSize off = static_cast<VkDeviceSize>(offset);
	vkCmdBindVertexBuffers(m_command_buffer, binding, 1, &raw, &off);
}

void VulkanCommandList::bind_index_buffer(IRHIBuffer &buffer, IndexFormat format, Uint64 offset) {
	auto &vk_buf = static_cast<VulkanBuffer &>(buffer);
	VkIndexType index_type = (format == IndexFormat::UInt32) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
	vkCmdBindIndexBuffer(m_command_buffer, vk_buf.get_buffer(), static_cast<VkDeviceSize>(offset), index_type);
}

// Draw commands

void VulkanCommandList::draw(Uint32 vertex_count, Uint32 instance_count, Uint32 first_vertex, Uint32 first_instance) {
	vkCmdDraw(m_command_buffer, vertex_count, instance_count, first_vertex, first_instance);
}

void VulkanCommandList::draw_indexed(Uint32 index_count, Uint32 instance_count, Uint32 first_index, Int32 vertex_offset,
									 Uint32 first_instance) {
	vkCmdDrawIndexed(m_command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void VulkanCommandList::draw_indirect(IRHIBuffer &buffer, Uint64 offset, Uint32 draw_count, Uint32 stride) {
	auto &vk_buf = static_cast<VulkanBuffer &>(buffer);
	vkCmdDrawIndirect(m_command_buffer, vk_buf.get_buffer(), static_cast<VkDeviceSize>(offset), draw_count, stride);
}

void VulkanCommandList::draw_indexed_indirect(IRHIBuffer &buffer, Uint64 offset, Uint32 draw_count, Uint32 stride) {
	auto &vk_buf = static_cast<VulkanBuffer &>(buffer);
	vkCmdDrawIndexedIndirect(m_command_buffer, vk_buf.get_buffer(), static_cast<VkDeviceSize>(offset), draw_count,
							 stride);
}

void VulkanCommandList::copy_buffer_to_texture(IRHIBuffer &src, IRHITexture &dst, Uint32 width, Uint32 height,
											   Uint32 dst_array_layer, Uint32 dst_mip_level) {
	auto &vk_buf = static_cast<VulkanBuffer &>(src);
	auto &vk_tex = static_cast<VulkanTexture &>(dst);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = dst_mip_level;
	region.imageSubresource.baseArrayLayer = dst_array_layer;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { width, height, 1 };

	vkCmdCopyBufferToImage(m_command_buffer, vk_buf.get_buffer(), vk_tex.get_image(),
						   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void VulkanCommandList::copy_texture_to_buffer(IRHITexture &src, IRHIBuffer &dst, Uint32 width, Uint32 height,
											   Uint32 src_array_layer, Uint32 src_mip_level, Int32 src_offset_x,
											   Int32 src_offset_y) {
	auto &vk_tex = static_cast<VulkanTexture &>(src);
	auto &vk_buf = static_cast<VulkanBuffer &>(dst);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = src_mip_level;
	region.imageSubresource.baseArrayLayer = src_array_layer;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { src_offset_x, src_offset_y, 0 };
	region.imageExtent = { width, height, 1 };

	vkCmdCopyImageToBuffer(m_command_buffer, vk_tex.get_image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						   vk_buf.get_buffer(), 1, &region);
}

void VulkanCommandList::fill_buffer(IRHIBuffer &buffer, Uint64 offset, Uint64 size, Uint32 value) {
	auto &vk_buf = static_cast<VulkanBuffer &>(buffer);
	vkCmdFillBuffer(m_command_buffer, vk_buf.get_buffer(), static_cast<VkDeviceSize>(offset),
					static_cast<VkDeviceSize>(size), value);
}

void VulkanCommandList::dispatch(Uint32 x, Uint32 y, Uint32 z) {
	vkCmdDispatch(m_command_buffer, x, y, z);
}

// Debug markers

void VulkanCommandList::reset_queries(IRHIQueryPool &pool, Uint32 first, Uint32 count) {
	const auto &vk_pool = static_cast<VulkanQueryPool &>(pool);
	if (vk_pool.get_handle() != VK_NULL_HANDLE && count > 0) {
		vkCmdResetQueryPool(m_command_buffer, vk_pool.get_handle(), first, count);
	}
}

void VulkanCommandList::write_timestamp(IRHIQueryPool &pool, Uint32 index) {
	const auto &vk_pool = static_cast<VulkanQueryPool &>(pool);
	if (vk_pool.get_handle() != VK_NULL_HANDLE && index < vk_pool.get_count()) {
		vkCmdWriteTimestamp(m_command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk_pool.get_handle(), index);
	}
}

void VulkanCommandList::push_debug_group(const char *name) {
	if (auto fn = m_device.get_debug_begin_label()) {
		VkDebugUtilsLabelEXT label{};
		label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		label.pLabelName = name;
		label.color[0] = 0.2F;
		label.color[1] = 0.6F;
		label.color[2] = 1.0F;
		label.color[3] = 1.0F;
		fn(m_command_buffer, &label);
	}
}

void VulkanCommandList::pop_debug_group() {
	if (auto fn = m_device.get_debug_end_label()) {
		fn(m_command_buffer);
	}
}

} // namespace Aquila::RHI
