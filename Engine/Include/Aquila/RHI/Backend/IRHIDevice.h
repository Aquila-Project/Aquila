#ifndef AQUILA_IRHI_DEVICE_H
#define AQUILA_IRHI_DEVICE_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/RHI/Backend/IRHIBuffer.h"
#include "Aquila/RHI/Backend/IRHICommandList.h"
#include "Aquila/RHI/Backend/IRHIDescriptors.h"
#include "Aquila/RHI/Backend/IRHIRenderpass.h"
#include "Aquila/RHI/Backend/IRHIPipeline.h"
#include "Aquila/RHI/Backend/IRHIQueryPool.h"
#include "Aquila/RHI/Backend/IRHISwapchain.h"
#include "Aquila/RHI/Backend/IRHITexture.h"
#include "Aquila/RHI/Backend/RHITypes.h"

namespace Aquila::RHI {

class IRHIDevice {
  public:
	virtual ~IRHIDevice() = default;

	IRHIDevice(const IRHIDevice &) = delete;
	IRHIDevice &operator=(const IRHIDevice &) = delete;
	IRHIDevice(IRHIDevice &&) = delete;
	IRHIDevice &operator=(IRHIDevice &&) = delete;

	[[nodiscard]] virtual Unique<IRHIBuffer> create_buffer(const BufferDesc &desc) = 0;
	[[nodiscard]] virtual Unique<IRHITexture> create_texture(const TextureDesc &desc) = 0;
	/// A one-off command list for submit() or submit_and_wait().
	[[nodiscard]] virtual Unique<IRHICommandList> create_command_list(CommandListType type,
																	const std::string &name = "") = 0;
	/// Wraps the frame slot's own command buffer, recycled when the slot comes back.
	/// Record the frame into it and hand it to submit_frame().
	[[nodiscard]] virtual Unique<IRHICommandList> create_frame_command_list(Uint32 slot) = 0;
	[[nodiscard]] virtual Unique<IRHISwapchain> create_swapchain(const SwapchainDesc &desc) = 0;
	[[nodiscard]] virtual Unique<IRHIRenderPass> create_render_pass(const RHI::RenderPassDesc &desc) = 0;
	[[nodiscard]] virtual Unique<IRHIQueryPool> create_timestamp_pool(Uint32 count) = 0;
	[[nodiscard]] virtual Unique<IRHIPipeline> create_graphics_pipeline(const GraphicsPipelineDesc &desc) = 0;
	[[nodiscard]] virtual Unique<IRHIPipeline> create_compute_pipeline(const ComputePipelineDesc &desc) = 0;
	[[nodiscard]] virtual Unique<IRHIDescriptorSetLayout>
	create_descriptor_set_layout(const DescriptorSetLayoutDesc &desc) = 0;
	[[nodiscard]] virtual Unique<IRHIDescriptorSet> allocate_descriptor_set(IRHIDescriptorSetLayout &layout) = 0;
	/// Records the copy into `cmd`, nothing runs yet.
	virtual void copy_buffer(IRHICommandList &cmd, IRHIBuffer &src, IRHIBuffer &dst, Uint64 size, Uint64 src_offset = 0,
							Uint64 dst_offset = 0) = 0;
	/// No fence and no wait, make sure it's done before you touch what it uses.
	virtual void submit(IRHICommandList &cmd) = 0;
	/// Ends `cmd` for you. With a swapchain it waits for the acquired image and presents it,
	/// without one it's an offscreen frame.
	virtual void submit_frame(IRHICommandList &cmd, IRHISwapchain *swapchain = nullptr, Uint32 image_index = 0) = 0;
	virtual void submit_and_wait(IRHICommandList &cmd) = 0;
	/// Clears the acquired image to `clear_color` and presents it, for frames with nothing to draw.
	virtual void present_frame(IRHISwapchain &swapchain, Uint32 image_index,
							  Vec4 clear_color = { 0.0F, 0.0F, 0.0F, 1.0F }) = 0;
	virtual void wait_idle() = 0;

	/// Records, submits and waits. Fine for uploads at load time, too slow for every frame.
	///
	/// @code
	/// device.execute_immediate(CommandListType::Graphics, [&](IRHICommandList &cmd) {
	///     cmd.copy_buffer_to_texture(staging, texture, width, height);
	/// });
	/// @endcode
	template <typename Func> void execute_immediate(CommandListType type, Func &&func) {
		auto cmd = create_command_list(type, "ImmediateCmd");
		cmd->begin();
		func(*cmd);
		cmd->end();
		submit_and_wait(*cmd);
	}

  protected:
	IRHIDevice() = default;
};

} // namespace Aquila::RHI
#endif
