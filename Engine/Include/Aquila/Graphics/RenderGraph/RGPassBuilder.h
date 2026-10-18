#pragma once
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Graphics/RenderGraph/RGTypes.h"
#include "Aquila/Graphics/RenderGraph/RGRegistry.h"

namespace Aquila::GFX {
class GfxCommandList;
}

namespace Aquila::Graphics::RG {

struct RGColorAttachment {
	RGTextureHandle handle;
	AttachmentLoadOp load_op = AttachmentLoadOp::Clear;
	AttachmentStoreOp store_op = AttachmentStoreOp::Store;
	ClearColor clear = {};
};

struct RGDepthAttachment {
	RGTextureHandle handle;
	AttachmentLoadOp depth_load_op = AttachmentLoadOp::Clear;
	AttachmentStoreOp depth_store_op = AttachmentStoreOp::Store;
	AttachmentLoadOp stencil_load_op = AttachmentLoadOp::DontCare;
	AttachmentStoreOp stencil_store_op = AttachmentStoreOp::DontCare;
	bool read_only = false; // depth-read / stencil-read layouts
	ClearDepth clear = {};
};

// Per-resource access records (used by the executor for barrier emission)
struct RGTextureAccess {
	RGTextureAccess(RGTextureHandle handle, ResourceState state) : handle(handle), state(state) {}
	RGTextureHandle handle;
	ResourceState state;
};

struct RGBufferAccess {
	RGBufferAccess(RGBufferHandle handle, ResourceState state) : handle(handle), state(state) {}
	RGBufferHandle handle;
	ResourceState state;
};
struct RGPassData {
	std::string name;

	// Fine-grained resource accesses (for barrier / hazard tracking).
	std::vector<RGTextureAccess> texture_reads;
	std::vector<RGTextureAccess> texture_writes;
	std::vector<RGBufferAccess> buffer_reads;
	std::vector<RGBufferAccess> buffer_writes;

	// Renderpass attachment descriptions (empty = compute / copy pass).
	std::vector<RGColorAttachment> color_attachments;
	RGDepthAttachment depth_attachment = {};
	bool has_depth_attachment = false;

	// The execute lambda called by the executor with a resolved command list.
	Delegate<void(GFX::GfxCommandList &, RGRegistry &)> render_pass_execute;

	// When true the culling step keeps this pass alive even if it has no
	// graph-tracked outputs (e.g. a swapchain blit that writes to an external image).
	bool has_side_effect = false;

	// Set when ReadBuffer/ReadTexture is called with an invalid handle.
	// The culling step will never mark this pass alive, regardless of downstream demand.
	bool has_unsatisfied_dep = false;
};

class RGPassBuilder {
  public:
	// Not user-constructible; the RenderGraph creates one per AddPass call.
	explicit RGPassBuilder(std::string_view pass_name, RGRegistry &registry);

	/// Declare a sampled  read.
	/// Returns the same handle (reads don't version).
	RGTextureHandle read_texture(RGTextureHandle handle, ResourceState state = ResourceState::ShaderRead);

	/// Declare a texture write and returns the NEW versioned handle.
	/// The caller MUST replace their local handle with the returned one.
	[[nodiscard]] RGTextureHandle write_texture(RGTextureHandle handle,
												ResourceState state = ResourceState::ColorAttachment);

	/// Declare a buffer  read.
	RGBufferHandle read_buffer(RGBufferHandle handle, ResourceState state = ResourceState::ShaderRead);

	/// Declare a buffer write.
	/// Returns the NEW versioned handle; caller must replace their local copy.
	[[nodiscard]] RGBufferHandle write_buffer(RGBufferHandle handle,
											  ResourceState state = ResourceState::UnorderedAccess);

	/// Declare a depth-stencil attachment.
	/// readOnly = true -> DepthRead layout, still usable as SRV in the same pass.
	/// Returns handle (no version bump for read-only; bumps for read-write).
	RGTextureHandle set_depth_attachment(RGTextureHandle handle, AttachmentLoadOp depth_load = AttachmentLoadOp::Clear,
										 AttachmentStoreOp depth_store = AttachmentStoreOp::Store,
										 AttachmentLoadOp stencil_load = AttachmentLoadOp::DontCare,
										 AttachmentStoreOp stencil_store = AttachmentStoreOp::DontCare,
										 bool read_only = false, ClearDepth clear = {});

	/// Declare a color attachment at a given slot index.
	/// Internally calls WriteTexture — returns the new versioned handle.
	[[nodiscard]] RGTextureHandle set_color_attachment(Uint32 slot, RGTextureHandle handle,
													   AttachmentLoadOp load_op = AttachmentLoadOp::Clear,
													   AttachmentStoreOp store_op = AttachmentStoreOp::Store,
													   ClearColor clear = {});

	// Mark this pass as having an external side effect (e.g. swapchain present).
	// Prevents the culling step from removing it even when it has no tracked outputs.
	void mark_as_side_effect() { m_data.has_side_effect = true; }

	// Called by RenderGraph after the setup lambda returns.
	RGPassData &&take_data() { return std::move(m_data); }

  private:
	RGRegistry &m_registry;
	RGPassData m_data;

	// Ensure a handle isn't double-declared with conflicting states.
	void assert_no_duplicate_texture_access(RGTextureHandle handle, bool is_write) const;
	void assert_no_duplicate_buffer_access(RGBufferHandle handle, bool is_write) const;
};

} // namespace Aquila::Graphics::RG
