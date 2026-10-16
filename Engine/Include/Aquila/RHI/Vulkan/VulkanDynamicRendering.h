#ifndef AQUILA_VULKAN_DYNAMIC_RENDERING_H
#define AQUILA_VULKAN_DYNAMIC_RENDERING_H

#include "GraphicsPCH.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/Color.h"
#include "Aquila/RHI/Backend/RHITypes.h"
#include "Aquila/RHI/Vulkan/VulkanCommandList.h"
#include "Aquila/RHI/Vulkan/VulkanFormatUtils.h"

namespace Aquila::RHI {

class VulkanDynamicRendering {
  public:
	struct ColorAttachmentDesc {
		VkImageView view = VK_NULL_HANDLE;
		VkImageView resolve_view = VK_NULL_HANDLE;
		VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		bool clear = true;
		Vec4 clear_color = { Foundation::Color::BLACK_V, 1.F };
	};

	struct DepthAttachmentDesc {
		VkImageView view = VK_NULL_HANDLE;
		VkImageView resolve_view = VK_NULL_HANDLE;
		VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		bool clear = true;
		float clear_depth = 1.F;
		bool read_only = false;
	};

	struct BeginDesc {
		Uint32 width = 0;
		Uint32 height = 0;
		std::vector<ColorAttachmentDesc> color_attachments;
		std::optional<DepthAttachmentDesc> depth_attachment;
	};

	/// Also sets a full size viewport and scissor.
	static void begin(VulkanCommandList &cmd, const BeginDesc &desc) {
		std::vector<VkRenderingAttachmentInfo> color_infos;
		color_infos.reserve(desc.color_attachments.size());

		for (const auto &c : desc.color_attachments) {
			VkRenderingAttachmentInfo info{};
			info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			info.imageView = c.view;
			info.imageLayout = c.layout;
			info.loadOp = c.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
			info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			info.clearValue.color = { { c.clear_color.r, c.clear_color.g, c.clear_color.b, c.clear_color.a } };

			if (c.resolve_view != VK_NULL_HANDLE) {
				info.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
				info.resolveImageView = c.resolve_view;
				info.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}

			color_infos.push_back(info);
		}

		VkRenderingAttachmentInfo depth_info{};
		if (desc.depth_attachment.has_value()) {
			const auto &d = *desc.depth_attachment;
			depth_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			depth_info.imageView = d.view;
			depth_info.imageLayout = d.read_only ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : d.layout;
			depth_info.loadOp = d.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
			depth_info.storeOp = d.read_only ? VK_ATTACHMENT_STORE_OP_NONE : VK_ATTACHMENT_STORE_OP_STORE;
			depth_info.clearValue.depthStencil = { .depth = d.clear_depth, .stencil = 0 };
		}

		VkRenderingInfo rendering_info{};
		rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		rendering_info.renderArea.extent = { .width = desc.width, .height = desc.height };
		rendering_info.layerCount = 1;
		rendering_info.colorAttachmentCount = static_cast<Uint32>(color_infos.size());
		rendering_info.pColorAttachments = color_infos.empty() ? nullptr : color_infos.data();
		rendering_info.pDepthAttachment = desc.depth_attachment.has_value() ? &depth_info : nullptr;

		vkCmdBeginRendering(cmd.get_handle(), &rendering_info);

		VkViewport viewport{};
		viewport.width = static_cast<float>(desc.width);
		viewport.height = static_cast<float>(desc.height);
		viewport.maxDepth = 1.F;
		vkCmdSetViewport(cmd.get_handle(), 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = { .width = desc.width, .height = desc.height };
		vkCmdSetScissor(cmd.get_handle(), 0, 1, &scissor);
	}

	static void end(VulkanCommandList &cmd) { vkCmdEndRendering(cmd.get_handle()); }
};

struct PipelineRenderingFormats {
	std::vector<VkFormat> color_formats;
	VkFormat depth_format = VK_FORMAT_UNDEFINED;

	static PipelineRenderingFormats g_buffer() {
		return { .color_formats = { VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT,
									VK_FORMAT_R8G8B8A8_UNORM },
				 .depth_format = VK_FORMAT_D32_SFLOAT };
	}
	static PipelineRenderingFormats sky_box() {
		return { .color_formats = { VK_FORMAT_R32G32B32A32_SFLOAT }, .depth_format = VK_FORMAT_D32_SFLOAT };
	}
	static PipelineRenderingFormats single_color(VkFormat format = VK_FORMAT_R8G8B8A8_UNORM) {
		return { .color_formats = { format }, .depth_format = VK_FORMAT_UNDEFINED };
	}
	static PipelineRenderingFormats depth_only(VkFormat depth_format = VK_FORMAT_D32_SFLOAT) {
		return { .color_formats = {}, .depth_format = depth_format };
	}
	static PipelineRenderingFormats lighting() {
		return { .color_formats = { VK_FORMAT_R16G16B16A16_SFLOAT }, .depth_format = VK_FORMAT_UNDEFINED };
	}
	static PipelineRenderingFormats composite() {
		return { .color_formats = { VK_FORMAT_R16G16B16A16_SFLOAT }, .depth_format = VK_FORMAT_UNDEFINED };
	}
	static PipelineRenderingFormats shadow() { return depth_only(); }
	static PipelineRenderingFormats gizmo() {
		return { .color_formats = { VK_FORMAT_R16G16B16A16_SFLOAT }, .depth_format = VK_FORMAT_D32_SFLOAT };
	}
	static PipelineRenderingFormats custom(const std::vector<VkFormat> &color_formats,
										   VkFormat depth_format = VK_FORMAT_UNDEFINED) {
		return { .color_formats = color_formats, .depth_format = depth_format };
	}

	[[nodiscard]] VkPipelineRenderingCreateInfo get_create_info() const {
		VkPipelineRenderingCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		info.colorAttachmentCount = static_cast<Uint32>(color_formats.size());
		info.pColorAttachmentFormats = color_formats.empty() ? nullptr : color_formats.data();
		info.depthAttachmentFormat = depth_format;
		return info;
	}
};

} // namespace Aquila::RHI
#endif
