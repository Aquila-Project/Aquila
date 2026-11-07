#include "Aquila/RHI/Vulkan/VulkanPipeline.h"
#include "Aquila/RHI/Vulkan/VulkanDeletionQueue.h"

#include "Aquila/RHI/Vulkan/VulkanCommandList.h"
#include "Aquila/RHI/Vulkan/VulkanDevice.h"

namespace Aquila::RHI {

VulkanPipeline::VulkanPipeline(VulkanDevice &device, const std::vector<VkPipelineShaderStageCreateInfo> &stages,
							   const VulkanPipelineConfig &config_info)
	: m_device(device), m_layout(config_info.pipeline_layout) {
	create_pipeline_from_stages(stages, config_info);
}

VulkanPipeline::~VulkanPipeline() {
	auto &dq = m_device.get_deletion_queue();
	if (m_graphics_pipeline != VK_NULL_HANDLE) {
		dq.queue_deletion(m_graphics_pipeline);
		m_graphics_pipeline = VK_NULL_HANDLE;
	}
	if (m_layout != VK_NULL_HANDLE) {
		dq.queue_deletion(m_layout);
		m_layout = VK_NULL_HANDLE;
	}
}

void VulkanPipeline::bind(IRHICommandList &cmd) {
	vkCmdBindPipeline(static_cast<VulkanCommandList &>(cmd).get_handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
					  m_graphics_pipeline);
}

void VulkanPipeline::create_pipeline_from_stages(const std::vector<VkPipelineShaderStageCreateInfo> &stages,
											  const VulkanPipelineConfig &config_info) {
	AQUILA_ASSERT(config_info.pipeline_layout != VK_NULL_HANDLE, "Pipeline layout must be set in VulkanPipelineConfig");
	AQUILA_ASSERT(!stages.empty(), "At least one shader stage is required");

	VkPipelineVertexInputStateCreateInfo vertex_input_info{};
	vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(config_info.binding_descriptions.size());
	vertex_input_info.pVertexBindingDescriptions = config_info.binding_descriptions.data();
	vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(config_info.attribute_descriptions.size());
	vertex_input_info.pVertexAttributeDescriptions = config_info.attribute_descriptions.data();

	uint32_t color_attachment_count = config_info.color_formats.empty()
		? static_cast<uint32_t>(config_info.color_blend_attachments.size())
		: static_cast<uint32_t>(config_info.color_formats.size());

	VkPipelineColorBlendStateCreateInfo color_blending{};
	color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending.logicOpEnable = VK_FALSE;
	color_blending.logicOp = VK_LOGIC_OP_COPY;
	color_blending.attachmentCount = color_attachment_count;
	color_blending.pAttachments =
		config_info.color_blend_attachments.empty() ? nullptr : config_info.color_blend_attachments.data();

	VkPipelineRenderingCreateInfo rendering_create_info{};
	rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	rendering_create_info.pNext = config_info.p_next;
	rendering_create_info.colorAttachmentCount = static_cast<uint32_t>(config_info.color_formats.size());
	rendering_create_info.pColorAttachmentFormats =
		config_info.color_formats.empty() ? nullptr : config_info.color_formats.data();
	rendering_create_info.depthAttachmentFormat = config_info.depth_format;
	rendering_create_info.stencilAttachmentFormat = config_info.stencil_format;

	bool use_dynamic_rendering = !config_info.color_formats.empty() || config_info.depth_format != VK_FORMAT_UNDEFINED;

	VkGraphicsPipelineCreateInfo pipeline_info{};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.pNext = use_dynamic_rendering ? &rendering_create_info : config_info.p_next;
	pipeline_info.stageCount = static_cast<uint32_t>(stages.size());
	pipeline_info.pStages = stages.data();
	pipeline_info.pVertexInputState = &vertex_input_info;
	pipeline_info.pInputAssemblyState = &config_info.input_assembly_info;
	pipeline_info.pViewportState = &config_info.viewport_info;
	pipeline_info.pRasterizationState = &config_info.rasterization_info;
	pipeline_info.pMultisampleState = &config_info.multisample_info;
	pipeline_info.pDepthStencilState = &config_info.depth_stencil_info;
	pipeline_info.pColorBlendState = &color_blending;
	pipeline_info.pDynamicState = &config_info.dynamic_state_create_info;
	pipeline_info.layout = config_info.pipeline_layout;
	pipeline_info.renderPass = config_info.render_pass;
	pipeline_info.subpass = 0;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_info.basePipelineIndex = -1;

	AQUILA_VULKAN_CHECK(vkCreateGraphicsPipelines(m_device.get_device(), m_device.get_pipeline_cache(), 1, &pipeline_info,
												  nullptr, &m_graphics_pipeline));
}

void VulkanPipeline::default_pipeline_config(VulkanPipelineConfig &config_info) {
	config_info.input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	config_info.input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	config_info.input_assembly_info.primitiveRestartEnable = VK_FALSE;

	config_info.viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	config_info.viewport_info.viewportCount = 1;
	config_info.viewport_info.pViewports = nullptr;
	config_info.viewport_info.scissorCount = 1;
	config_info.viewport_info.pScissors = nullptr;

	config_info.rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	config_info.rasterization_info.depthClampEnable = VK_FALSE;
	config_info.rasterization_info.rasterizerDiscardEnable = VK_FALSE;
	config_info.rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
	config_info.rasterization_info.lineWidth = 1.0F;
	config_info.rasterization_info.cullMode = VK_CULL_MODE_NONE;
	config_info.rasterization_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
	config_info.rasterization_info.depthBiasEnable = VK_FALSE;

	config_info.multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	config_info.multisample_info.sampleShadingEnable = VK_FALSE;
	config_info.multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	config_info.multisample_info.minSampleShading = 1.0F;
	config_info.multisample_info.pSampleMask = nullptr;
	config_info.multisample_info.alphaToCoverageEnable = VK_FALSE;
	config_info.multisample_info.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState default_attachment{};
	default_attachment.blendEnable = VK_TRUE;
	default_attachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	default_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	default_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	default_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	default_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	default_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	default_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

	config_info.color_blend_attachments.clear();
	config_info.color_blend_attachments.push_back(default_attachment);

	config_info.color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	config_info.color_blend_info.logicOpEnable = VK_FALSE;
	config_info.color_blend_info.logicOp = VK_LOGIC_OP_COPY;
	config_info.color_blend_info.attachmentCount = static_cast<uint32_t>(config_info.color_blend_attachments.size());
	config_info.color_blend_info.pAttachments = config_info.color_blend_attachments.data();

	config_info.depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	config_info.depth_stencil_info.depthTestEnable = VK_TRUE;
	config_info.depth_stencil_info.depthWriteEnable = VK_TRUE;
	config_info.depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	config_info.depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
	config_info.depth_stencil_info.minDepthBounds = 0.0F;
	config_info.depth_stencil_info.maxDepthBounds = 1.0F;
	config_info.depth_stencil_info.stencilTestEnable = VK_FALSE;
	config_info.depth_stencil_info.front = {};
	config_info.depth_stencil_info.back = {};

	config_info.dynamic_state_enables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	config_info.dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	config_info.dynamic_state_create_info.pDynamicStates = config_info.dynamic_state_enables.data();
	config_info.dynamic_state_create_info.dynamicStateCount = static_cast<uint32_t>(config_info.dynamic_state_enables.size());
	config_info.dynamic_state_create_info.flags = 0;

	if (config_info.custom_vertex_layout.has_value()) {
		auto &layout = config_info.custom_vertex_layout.value();
		config_info.binding_descriptions = { { 0, layout.stride, VK_VERTEX_INPUT_RATE_VERTEX } };
		config_info.attribute_descriptions.clear();
		for (auto &a : layout.attributes) {
			config_info.attribute_descriptions.push_back({ a.location, a.binding, to_vk_format(a.format), a.offset });
		}
	} else {
		config_info.binding_descriptions = Vertex::get_binding_descriptions();
		config_info.attribute_descriptions = Vertex::get_attribute_descriptions();
	}
}

} // namespace Aquila::RHI
