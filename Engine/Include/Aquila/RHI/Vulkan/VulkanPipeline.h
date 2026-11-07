#ifndef AQUILA_VULKAN_PIPELINE_H
#define AQUILA_VULKAN_PIPELINE_H

#include "GraphicsPCH.h"
#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/RHI/Backend/IRHIPipeline.h"
#include "Aquila/RHI/Vulkan/VulkanVertex.h"

namespace Aquila::RHI {

class VulkanDevice;
class VulkanCommandList;

struct VulkanPipelineConfig {
	VulkanPipelineConfig() = default;
	AQUILA_NONCOPYABLE(VulkanPipelineConfig);

	std::vector<VkVertexInputBindingDescription> binding_descriptions{};
	std::vector<VkVertexInputAttributeDescription> attribute_descriptions{};
	VkPipelineViewportStateCreateInfo viewport_info{};
	VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
	VkPipelineRasterizationStateCreateInfo rasterization_info{};
	VkPipelineMultisampleStateCreateInfo multisample_info{};
	std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments{};
	VkPipelineColorBlendStateCreateInfo color_blend_info{};
	VkPipelineDepthStencilStateCreateInfo depth_stencil_info{};
	std::vector<VkDynamicState> dynamic_state_enables{};
	VkPipelineDynamicStateCreateInfo dynamic_state_create_info{};
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	std::optional<VertexBindingDesc> custom_vertex_layout;
	const void *p_next = nullptr;
	VkRenderPass render_pass = VK_NULL_HANDLE;
	std::vector<VkFormat> color_formats;
	VkFormat depth_format = VK_FORMAT_UNDEFINED;
	VkFormat stencil_format = VK_FORMAT_UNDEFINED;
};

class VulkanPipeline final : public IRHIPipeline {
  public:
	VulkanPipeline(VulkanDevice &device, const std::vector<VkPipelineShaderStageCreateInfo> &stages,
				   const VulkanPipelineConfig &config_info);
	~VulkanPipeline() override;

	AQUILA_NONCOPYABLE(VulkanPipeline);

	// IRHIPipeline
	void bind(IRHICommandList &cmd) override;
	[[nodiscard]] PipelineBindPoint get_bind_point() const override { return PipelineBindPoint::Graphics; }

	// Vulkan-specific
	[[nodiscard]] VkPipeline get_pipeline() const { return m_graphics_pipeline; }
	[[nodiscard]] VkPipelineLayout get_layout() const { return m_layout; }

	static void default_pipeline_config(VulkanPipelineConfig &config_info);

  private:
	void create_pipeline_from_stages(const std::vector<VkPipelineShaderStageCreateInfo> &stages,
									 const VulkanPipelineConfig &config_info);

	VulkanDevice &m_device;
	VkPipeline m_graphics_pipeline = VK_NULL_HANDLE;
	VkPipelineLayout m_layout = VK_NULL_HANDLE;
};

} // namespace Aquila::RHI
#endif
