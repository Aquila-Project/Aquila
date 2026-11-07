#include "Aquila/RHI/Vulkan/VulkanRenderPass.h"
#include "Aquila/RHI/Vulkan/VulkanDevice.h"
#include "Aquila/RHI/Vulkan/VulkanCommandList.h"
#include "Aquila/RHI/Vulkan/VulkanSwapchain.h"
#include "Aquila/RHI/Vulkan/VulkanTexture.h"
#include "Aquila/RHI/Vulkan/VulkanFormatUtils.h"
#include "Aquila/Foundation/Macros.h"

namespace Aquila::RHI {

static VkImageMemoryBarrier make_barrier(VkImage image, VkImageAspectFlags aspect, VkImageLayout old_layout,
										 VkImageLayout new_layout, VkAccessFlags src_access, VkAccessFlags dst_access) {
	VkImageMemoryBarrier b{};
	b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	b.oldLayout = old_layout;
	b.newLayout = new_layout;
	b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.image = image;
	b.subresourceRange = { aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
	b.srcAccessMask = src_access;
	b.dstAccessMask = dst_access;
	return b;
}

VulkanRenderPass::VulkanRenderPass(VulkanDevice &device, const RenderPassDesc &desc) : m_device(device), m_desc(desc) {}

void VulkanRenderPass::issue_pre_barriers(VulkanCommandList &cmd, const VulkanSwapchain *swapchain,
										  Uint32 image_index) const {
	std::vector<VkImageMemoryBarrier> barriers;
	barriers.reserve(4);

	VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

	if (m_desc.use_swapchain) {
		AQUILA_ASSERT(swapchain, "useSwapchain=true but no swapchain passed to Begin()");
		barriers.push_back(make_barrier(swapchain->get_image(image_index), VK_IMAGE_ASPECT_COLOR_BIT,
										VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
										VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));
	} else {
		for (const auto &att : m_desc.color_attachments) {
			if (att.texture == nullptr) {
				continue;
			}
			if (att.resolve_texture != nullptr) {
				auto &vk_resolve = static_cast<VulkanTexture &>(*att.resolve_texture);
				barriers.push_back(make_barrier(vk_resolve.get_image(), VK_IMAGE_ASPECT_COLOR_BIT,
												VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
												VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));
			}

			auto &vk_tex = static_cast<VulkanTexture &>(*att.texture);

			VkImageLayout old_layout = (att.load_op == AttachmentLoadOp::Load)
				? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				: VK_IMAGE_LAYOUT_UNDEFINED;
			VkAccessFlags src_access = (att.load_op == AttachmentLoadOp::Load) ? VK_ACCESS_SHADER_READ_BIT : 0;

			barriers.push_back(make_barrier(
				vk_tex.get_image(), VK_IMAGE_ASPECT_COLOR_BIT, old_layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				src_access, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT));
		}

		if (m_desc.use_swapchain_as_resolve) {
			AQUILA_ASSERT(swapchain, "useSwapchainAsResolve=true but no swapchain passed to Begin()");
			barriers.push_back(make_barrier(swapchain->get_image(image_index), VK_IMAGE_ASPECT_COLOR_BIT,
											VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
											VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));
		}
	}

	if (m_desc.depth_attachment.has_value()) {
		const auto &d = *m_desc.depth_attachment;

		VkImage depth_image = VK_NULL_HANDLE;
		VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (d.texture != nullptr) {
			auto &vk_depth = static_cast<VulkanTexture &>(*d.texture);
			depth_image = vk_depth.get_image();
			aspect = to_vk_image_aspect(vk_depth.get_format());
		} else if (m_desc.use_swapchain && (swapchain != nullptr)) {
			depth_image = swapchain->get_depth_image(image_index);
		}

		if (depth_image != VK_NULL_HANDLE) {
			VkImageLayout old_layout = (d.depth_load_op == AttachmentLoadOp::Load)
				? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
				: VK_IMAGE_LAYOUT_UNDEFINED;
			VkAccessFlags src_access = (d.depth_load_op == AttachmentLoadOp::Load) ? VK_ACCESS_SHADER_READ_BIT : 0;
			VkImageLayout new_layout = d.read_only ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
												   : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
			VkAccessFlags dst_access = d.read_only
				? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
				: (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

			barriers.push_back(make_barrier(depth_image, aspect, old_layout, new_layout, src_access, dst_access));
		}
	}

	if (!barriers.empty()) {
		vkCmdPipelineBarrier(cmd.get_handle(), src_stage, dst_stage, 0, 0, nullptr, 0, nullptr,
							 static_cast<Uint32>(barriers.size()), barriers.data());
	}
}

void VulkanRenderPass::issue_post_barriers(VulkanCommandList &cmd) const {
	std::vector<VkImageMemoryBarrier> barriers;
	barriers.reserve(4);

	VkPipelineStageFlags src_stage =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

	if (m_desc.use_swapchain || m_desc.use_swapchain_as_resolve) {
		AQUILA_ASSERT(m_active_swapchain, "useSwapchain/useSwapchainAsResolve but swapchain is null during End()");
		barriers.push_back(make_barrier(m_active_swapchain->get_image(m_swapchain_image_index),
										VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
										VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0));
	}

	if (!m_desc.use_swapchain) {
		for (const auto &att : m_desc.color_attachments) {
			if ((att.texture == nullptr) || att.store_op == AttachmentStoreOp::DontCare) {
				continue;
			}
			if (att.resolve_texture != nullptr) {
				auto &vk_resolve = static_cast<VulkanTexture &>(*att.resolve_texture);
				barriers.push_back(make_barrier(vk_resolve.get_image(), VK_IMAGE_ASPECT_COLOR_BIT,
												VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
												VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
												VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT));
			}

			auto &vk_tex = static_cast<VulkanTexture &>(*att.texture);

			if ((to_vk_image_usage(vk_tex.get_desc().usage) & VK_IMAGE_USAGE_SAMPLED_BIT) == 0u) {
				continue;
			}

			barriers.push_back(make_barrier(vk_tex.get_image(), VK_IMAGE_ASPECT_COLOR_BIT,
											VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
											VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
											VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT));
		}
	}

	if (m_desc.depth_attachment.has_value()) {
		const auto &d = *m_desc.depth_attachment;
		if ((d.texture != nullptr) && d.depth_store_op == AttachmentStoreOp::Store) {
			auto &vk_depth = static_cast<VulkanTexture &>(*d.texture);
			VkImageAspectFlags aspect = to_vk_image_aspect(vk_depth.get_format());
			VkImageLayout old_layout = d.read_only ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
												   : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
			barriers.push_back(make_barrier(
				vk_depth.get_image(), aspect, old_layout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
				d.read_only ? 0 : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT));
		}
	}

	if (!barriers.empty()) {
		vkCmdPipelineBarrier(cmd.get_handle(), src_stage, dst_stage, 0, 0, nullptr, 0, nullptr,
							 static_cast<Uint32>(barriers.size()), barriers.data());
	}
}

void VulkanRenderPass::begin(IRHICommandList &cmd, IRHISwapchain *swapchain, Uint32 image_index) {
	AQUILA_ASSERT(!m_recording, "RenderPass already recording");
	m_recording = true;

	auto &vk_cmd = dynamic_cast<VulkanCommandList &>(cmd);
	m_active_swapchain = (swapchain != nullptr) ? &dynamic_cast<VulkanSwapchain &>(*swapchain) : nullptr;
	m_swapchain_image_index = image_index;

	if (!m_desc.external_barriers) {
		issue_pre_barriers(vk_cmd, m_active_swapchain, image_index);
	}

	Uint32 width = m_desc.width;
	Uint32 height = m_desc.height;

	std::vector<VkRenderingAttachmentInfo> color_infos;
	const auto add_color = [&color_infos](VkImageView view, VkImageView resolve_view,
										  const RenderPassColorAttachmentDesc &att) {
		VkRenderingAttachmentInfo info{};
		info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		info.imageView = view;
		info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		info.loadOp = att.load_op == AttachmentLoadOp::Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		info.clearValue.color = { { att.clear_color.r, att.clear_color.g, att.clear_color.b, att.clear_color.a } };
		if (resolve_view != VK_NULL_HANDLE) {
			info.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
			info.resolveImageView = resolve_view;
			info.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		color_infos.push_back(info);
	};

	if (m_desc.use_swapchain) {
		AQUILA_ASSERT(m_active_swapchain, "useSwapchain=true but no swapchain passed");
		width = m_active_swapchain->get_extent().width;
		height = m_active_swapchain->get_extent().height;

		const RenderPassColorAttachmentDesc &src =
			m_desc.color_attachments.empty() ? RenderPassColorAttachmentDesc{} : m_desc.color_attachments[0];
		add_color(m_active_swapchain->get_image_view(image_index), VK_NULL_HANDLE, src);
		m_color_format = TextureFormat::BGRA8;
	} else {
		if (m_desc.use_swapchain_as_resolve && (m_active_swapchain != nullptr)) {
			width = m_active_swapchain->get_extent().width;
			height = m_active_swapchain->get_extent().height;
		}

		for (const auto &att : m_desc.color_attachments) {
			AQUILA_ASSERT(att.texture, "Offscreen color attachment has null texture");
			auto &vk_tex = dynamic_cast<VulkanTexture &>(*att.texture);

			VkImageView resolve_view = VK_NULL_HANDLE;
			if (att.resolve_texture != nullptr) {
				resolve_view = dynamic_cast<VulkanTexture &>(*att.resolve_texture).get_image_view();
			} else if (m_desc.use_swapchain_as_resolve && (m_active_swapchain != nullptr)) {
				resolve_view = m_active_swapchain->get_image_view(image_index);
			}

			add_color(vk_tex.get_image_view(), resolve_view, att);
			m_color_format = m_desc.use_swapchain_as_resolve ? TextureFormat::BGRA8 : vk_tex.get_format();
			m_sample_count = vk_tex.get_sample_count();
		}
	}

	VkRenderingAttachmentInfo depth_info{};
	bool has_depth = false;
	if (m_desc.depth_attachment.has_value()) {
		const auto &d = *m_desc.depth_attachment;

		VkImageView depth_view = VK_NULL_HANDLE;
		if (d.texture != nullptr) {
			depth_view = dynamic_cast<VulkanTexture &>(*d.texture).get_image_view();
		} else if (m_desc.use_swapchain && (m_active_swapchain != nullptr)) {
			depth_view = m_active_swapchain->get_depth_image_view(image_index);
		}

		if (depth_view != VK_NULL_HANDLE) {
			has_depth = true;
			depth_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			depth_info.imageView = depth_view;
			depth_info.imageLayout = d.read_only ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
												 : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
			depth_info.loadOp =
				d.depth_load_op == AttachmentLoadOp::Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
			depth_info.storeOp = d.read_only ? VK_ATTACHMENT_STORE_OP_NONE : VK_ATTACHMENT_STORE_OP_STORE;
			depth_info.clearValue.depthStencil = { .depth = d.clear_depth, .stencil = 0 };
		}
	}

	VkRenderingInfo rendering_info{};
	rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	rendering_info.renderArea.extent = { .width = width, .height = height };
	rendering_info.layerCount = 1;
	rendering_info.colorAttachmentCount = static_cast<Uint32>(color_infos.size());
	rendering_info.pColorAttachments = color_infos.empty() ? nullptr : color_infos.data();
	rendering_info.pDepthAttachment = has_depth ? &depth_info : nullptr;
	vkCmdBeginRendering(vk_cmd.get_handle(), &rendering_info);

	m_width = width;
	m_height = height;

	// Default full-attachment viewport and scissor — caller can override via
	// cmd.SetViewport / cmd.SetScissor after Begin() returns.
	VkViewport viewport{
		.x = 0.0F,
		.y = 0.0F,
		.width = static_cast<float>(width),
		.height = static_cast<float>(height),
		.minDepth = 0.0F,
		.maxDepth = 1.0F,
	};
	vkCmdSetViewport(vk_cmd.get_handle(), 0, 1, &viewport);

	VkRect2D scissor{ .offset = { .x = 0, .y = 0 }, .extent = { .width = width, .height = height } };
	vkCmdSetScissor(vk_cmd.get_handle(), 0, 1, &scissor);
}

void VulkanRenderPass::end(IRHICommandList &cmd) {
	AQUILA_ASSERT(m_recording, "RenderPass not recording");
	auto &vk_cmd = static_cast<VulkanCommandList &>(cmd);

	vkCmdEndRendering(vk_cmd.get_handle());
	if (!m_desc.external_barriers) {
		issue_post_barriers(vk_cmd);
	}

	m_recording = false;
	m_active_swapchain = nullptr;
}

} // namespace Aquila::RHI
