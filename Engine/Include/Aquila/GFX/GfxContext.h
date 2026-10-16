#pragma once
#include "Aquila/RHI/Backend/IRHIDevice.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/GFX/GfxBuffer.h"
#include "Aquila/GFX/GfxTexture.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/GFX/GfxSwapchain.h"
#include "Aquila/GFX/GfxPipeline.h"
#include "Aquila/GFX/GfxRenderpass.h"
#include "Aquila/GFX/GfxDescriptorSet.h"

struct GLFWwindow;

namespace Aquila::GFX {

class GfxContext {
  public:
	static Unique<GfxContext> create(GLFWwindow &window);
	~GfxContext();
	AQUILA_NONCOPYABLE(GfxContext);
	AQUILA_NONMOVEABLE(GfxContext);

	[[nodiscard]] Ref<GfxBuffer> create_buffer(const RHI::BufferDesc &desc);
	[[nodiscard]] Ref<GfxTexture> create_texture(const RHI::TextureDesc &desc);
	[[nodiscard]] Ref<GfxSwapchain> create_swapchain(const RHI::SwapchainDesc &desc);
	[[nodiscard]] Ref<GfxPipeline> create_graphics_pipeline(const RHI::GraphicsPipelineDesc &desc);
	[[nodiscard]] Ref<GfxPipeline> create_compute_pipeline(const RHI::ComputePipelineDesc &desc);
	[[nodiscard]] Ref<GfxDescriptorSetLayout> create_descriptor_set_layout(const RHI::DescriptorSetLayoutDesc &desc);
	[[nodiscard]] Ref<GfxDescriptorSet> allocate_descriptor_set(GfxDescriptorSetLayout &layout);
	[[nodiscard]] Ref<GfxRenderPass> create_render_pass(const RHI::RenderPassDesc &desc);
	[[nodiscard]] Unique<RHI::IRHIQueryPool> create_timestamp_pool(Uint32 count);

	[[nodiscard]] Ref<GfxCommandList> create_command_list(RHI::CommandListType type, const std::string &name = {});
	[[nodiscard]] GfxCommandList &acquire_frame_command_list(Uint32 frame_slot);

	void copy_buffer(GfxBuffer &src, GfxBuffer &dst, Uint64 size, Uint64 src_offset = 0, Uint64 dst_offset = 0);
	void upload_texture_data(GfxTexture &dst, const void *data, Uint64 byte_size);
	void upload_texture_mips(GfxTexture &dst, const std::vector<std::vector<Uint8>> &levels);

	void destroy_immediate_texture(GfxTexture &texture);
	void destroy_immediate_buffer(GfxBuffer &buffer);

	// Records end-of-frame barriers, submits, and presents.
	// All Vulkan sync (semaphores, fences) is handled inside IRHIDevice.
	void submit_frame(GfxCommandList &cmd, GfxSwapchain *swapchain = nullptr, Uint32 image_index = 0);
	void submit_and_wait(GfxCommandList &cmd);

	template <typename Func> void execute_immediate(RHI::CommandListType type, Func &&func) {
		auto cmd = create_command_list(type, "ImmediateCmd");
		cmd->begin();
		func(*cmd);
		cmd->end();
		m_device->submit_and_wait(cmd->get_rhi());
	}
	void wait_idle();

	[[nodiscard]] RHI::IRHIDevice &get_device() { return *m_device; }

  private:
	explicit GfxContext(Unique<RHI::IRHIDevice> device);
	Unique<RHI::IRHIDevice> m_device;
	std::array<Unique<GfxCommandList>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_frame_command_lists;
};

} // namespace Aquila::GFX
