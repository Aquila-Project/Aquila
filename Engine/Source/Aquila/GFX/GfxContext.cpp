#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxBuffer.h"
#include "Aquila/GFX/GfxTexture.h"
#include "Aquila/RHI/RHIBackend.h"
#include "Aquila/Foundation/Macros.h"

namespace Aquila::GFX {

GfxContext::GfxContext(Unique<RHI::IRHIDevice> device) : m_device(std::move(device)) {
	for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
		m_frame_command_lists[i] = Unique<GfxCommandList>(new GfxCommandList(m_device->create_frame_command_list(i)));
	}
}
GfxContext::~GfxContext() = default;

Unique<GfxContext> GfxContext::create(GLFWwindow &window) {
	// NOTE : THEORETICALLY THIS IS WHERE WE WOULD SPLIT BY RHIBACKENDTYPE
	// TODO: when time comes -> define a RHIBackend.h file and set the Enums
	// for now we'll select API by hand, but in the future it would be nice to have an engine config
	//
	// in the RHIBackend.h we would have something like htis  :
	// Unique<IRHIDevice> CreateDevice(RHIBackendType backend, GLFWwindow &window) {
	//     switch (backend) {
	//      case RHIBackendType::Vulkan: return CreateVulkanBackend(window);
	//      case RHIBackendType::DirectX12: return CreateDX12Backend(window);
	//      case RHIBackendType::Metal: return CreateMetalBackend(window);
	//     }
	// }
	//
	//
	// and this should become :
	// Unique<GfxContext> GfxContext::Create(GLFWwindow &window, RHI::RHIBackendType backend) {
	//      return Unique<GfxContext>(new GfxContext(RHI::CreateDevice(backend, window)));
	// }
	return Unique<GfxContext>(new GfxContext(RHI::create_vulkan_backend(window)));
}

Ref<GfxBuffer> GfxContext::create_buffer(const RHI::BufferDesc &desc) {
	return Ref<GfxBuffer>(new GfxBuffer(m_device->create_buffer(desc)));
}
Ref<GfxTexture> GfxContext::create_texture(const RHI::TextureDesc &desc) {
	return Ref<GfxTexture>(new GfxTexture(m_device->create_texture(desc)));
}
Ref<GfxSwapchain> GfxContext::create_swapchain(const RHI::SwapchainDesc &desc) {
	return Ref<GfxSwapchain>(new GfxSwapchain(m_device->create_swapchain(desc)));
}
Ref<GfxPipeline> GfxContext::create_graphics_pipeline(const RHI::GraphicsPipelineDesc &desc) {
	return Ref<GfxPipeline>(new GfxPipeline(m_device->create_graphics_pipeline(desc)));
}
Ref<GfxPipeline> GfxContext::create_compute_pipeline(const RHI::ComputePipelineDesc &desc) {
	return Ref<GfxPipeline>(new GfxPipeline(m_device->create_compute_pipeline(desc)));
}
Ref<GfxDescriptorSetLayout> GfxContext::create_descriptor_set_layout(const RHI::DescriptorSetLayoutDesc &desc) {
	return Ref<GfxDescriptorSetLayout>(new GfxDescriptorSetLayout(m_device->create_descriptor_set_layout(desc)));
}
Ref<GfxDescriptorSet> GfxContext::allocate_descriptor_set(GfxDescriptorSetLayout &layout) {
	return Ref<GfxDescriptorSet>(new GfxDescriptorSet(m_device->allocate_descriptor_set(layout.get_rhi())));
}
Unique<RHI::IRHIQueryPool> GfxContext::create_timestamp_pool(Uint32 count) {
	return m_device->create_timestamp_pool(count);
}

Ref<GfxRenderPass> GfxContext::create_render_pass(const RHI::RenderPassDesc &desc) {
	return Ref<GfxRenderPass>(new GfxRenderPass(m_device->create_render_pass(desc)));
}
Ref<GfxCommandList> GfxContext::create_command_list(RHI::CommandListType type, const std::string &name) {
	return Ref<GfxCommandList>(new GfxCommandList(m_device->create_command_list(type, name)));
}

GfxCommandList &GfxContext::acquire_frame_command_list(Uint32 frame_slot) {
	AQUILA_ASSERT(frame_slot < SharedConstants::MAX_FRAMES_IN_FLIGHT, "Frame slot out of range");
	return *m_frame_command_lists[frame_slot];
}

void GfxContext::submit_frame(GfxCommandList &cmd, GfxSwapchain *swapchain, Uint32 image_index) {
	m_device->submit_frame(cmd.get_rhi(), (swapchain != nullptr) ? &swapchain->get_rhi() : nullptr, image_index);
}

void GfxContext::submit_and_wait(GfxCommandList &cmd) {
	if (cmd.is_recording()) {
		cmd.end();
	}
	m_device->submit_and_wait(cmd.get_rhi());
}

void GfxContext::upload_texture_data(GfxTexture &dst, const void *data, Uint64 byte_size) {
	RHI::BufferDesc staging_desc{};
	staging_desc.size = byte_size;
	staging_desc.usage = RHI::BufferUsage::TransferSrc;
	staging_desc.domain = RHI::MemoryDomain::CpuOnly;
	staging_desc.debug_name = "TextureUploadStaging";

	Ref<GfxBuffer> staging = create_buffer(staging_desc);
	staging->get_rhi().write(data, byte_size, 0);

	const Uint32 w = dst.get_width();
	const Uint32 h = dst.get_height();

	execute_immediate(RHI::CommandListType::Graphics, [&](GfxCommandList &cmd) {
		cmd.transition_texture(dst, RHI::ResourceState::Undefined, RHI::ResourceState::TransferDst);
		cmd.copy_buffer_to_texture(*staging, dst, w, h);
		cmd.transition_texture(dst, RHI::ResourceState::TransferDst, RHI::ResourceState::ShaderRead);
	});

	destroy_immediate_buffer(*staging);
}

void GfxContext::upload_texture_mips(GfxTexture &dst, const std::vector<std::vector<Uint8>> &levels) {
	std::vector<Ref<GfxBuffer>> staging;
	staging.reserve(levels.size());
	for (const auto &level : levels) {
		RHI::BufferDesc staging_desc{};
		staging_desc.size = level.size();
		staging_desc.usage = RHI::BufferUsage::TransferSrc;
		staging_desc.domain = RHI::MemoryDomain::CpuOnly;
		staging_desc.debug_name = "TextureMipUploadStaging";
		staging.push_back(create_buffer(staging_desc));
		staging.back()->get_rhi().write(level.data(), level.size(), 0);
	}

	execute_immediate(RHI::CommandListType::Graphics, [&](GfxCommandList &cmd) {
		cmd.transition_texture(dst, RHI::ResourceState::Undefined, RHI::ResourceState::TransferDst);
		for (Uint32 mip = 0; mip < static_cast<Uint32>(staging.size()); ++mip) {
			const Uint32 w = std::max(1U, dst.get_width() >> mip);
			const Uint32 h = std::max(1U, dst.get_height() >> mip);
			cmd.copy_buffer_to_texture(*staging[mip], dst, w, h, 0, mip);
		}
		cmd.transition_texture(dst, RHI::ResourceState::TransferDst, RHI::ResourceState::ShaderRead);
	});

	for (auto &buffer : staging) {
		destroy_immediate_buffer(*buffer);
	}
}

void GfxContext::copy_buffer(GfxBuffer &src, GfxBuffer &dst, Uint64 size, Uint64 src_offset, Uint64 dst_offset) {
	execute_immediate(RHI::CommandListType::Transfer, [&](GfxCommandList &cmd) {
		m_device->copy_buffer(cmd.get_rhi(), src.get_rhi(), dst.get_rhi(), size, src_offset, dst_offset);
	});
}

void GfxContext::wait_idle() {
	m_device->wait_idle();
}

void GfxContext::destroy_immediate_buffer(GfxBuffer &buffer) {
	buffer.destroy_immediate();
}

void GfxContext::destroy_immediate_texture(GfxTexture &texture) {
	texture.destroy_immediate();
}

} // namespace Aquila::GFX
