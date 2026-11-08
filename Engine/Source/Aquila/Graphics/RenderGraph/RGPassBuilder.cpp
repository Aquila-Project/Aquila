#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"

namespace Aquila::Graphics::RG {

static Uint32 slot_of(Uint32 id) {
	return id & 0x00FFFFFFu;
}

RGPassBuilder::RGPassBuilder(std::string_view pass_name, RGRegistry &registry) : m_registry(registry) {
	m_data.name = pass_name;
}

RGTextureHandle RGPassBuilder::read_texture(RGTextureHandle handle, ResourceState state) {
	if (!handle.is_valid()) {
		m_data.has_unsatisfied_dep = true;
		return handle;
	}

	// Same slot declared as a read twice is fine, e.g. depth-read attachment + SRV in the same pass.
	const Uint32 slot = slot_of(handle.id);
	auto slot_matches = [slot](const RGTextureAccess &a) { return slot_of(a.handle.id) == slot; };
	if (std::ranges::any_of(m_data.texture_reads, slot_matches)) {
		return handle;
	}

	assert_no_duplicate_texture_access(handle, /*isWrite=*/false);
	m_data.texture_reads.push_back({ handle, state });
	return handle;
}

RGTextureHandle RGPassBuilder::write_texture(RGTextureHandle handle, ResourceState state) {
	AQUILA_ASSERT(handle.is_valid(), "WriteTexture: invalid handle");
	assert_no_duplicate_texture_access(handle, /*isWrite=*/true);

	// Bump version in the registry — every write produces a new handle.
	RGTextureHandle new_handle = m_registry.write_texture(handle);
	m_data.texture_writes.push_back({ new_handle, state });
	return new_handle;
}

RGBufferHandle RGPassBuilder::read_buffer(RGBufferHandle handle, ResourceState state) {
	if (!handle.is_valid()) {
		m_data.has_unsatisfied_dep = true;
		return handle;
	}

	const Uint32 slot = slot_of(handle.id);
	auto slot_matches = [slot](const RGBufferAccess &a) { return slot_of(a.handle.id) == slot; };
	if (std::ranges::any_of(m_data.buffer_reads, slot_matches)) {
		return handle;
	}

	assert_no_duplicate_buffer_access(handle, /*isWrite=*/false);
	m_data.buffer_reads.push_back({ handle, state });
	return handle;
}

RGBufferHandle RGPassBuilder::write_buffer(RGBufferHandle handle, ResourceState state) {
	AQUILA_ASSERT(handle.is_valid(), "WriteBuffer: invalid handle");
	assert_no_duplicate_buffer_access(handle, /*isWrite=*/true);

	RGBufferHandle new_handle = m_registry.write_buffer(handle);
	m_data.buffer_writes.push_back({ new_handle, state });
	return new_handle;
}

RGTextureHandle RGPassBuilder::set_depth_attachment(RGTextureHandle handle, AttachmentLoadOp depth_load,
												  AttachmentStoreOp depth_store, AttachmentLoadOp stencil_load,
												  AttachmentStoreOp stencil_store, bool read_only, ClearDepth clear) {
	AQUILA_ASSERT(handle.is_valid(), "SetDepthAttachment: invalid handle");
	AQUILA_ASSERT(!m_data.has_depth_attachment, "A pass can only have one depth attachment");

	RGTextureHandle resolved_handle = handle;

	if (read_only) {
		// Read-only depth: register as a texture read with DepthRead state.
		// No version bump the resource isn't modified.
		resolved_handle = read_texture(handle, ResourceState::DepthRead);
	} else {
		// Read-write depth: counts as a write, bumps the version.
		resolved_handle = write_texture(handle, ResourceState::DepthWrite);
	}

	m_data.depth_attachment = RGDepthAttachment{
		.handle = resolved_handle,
		.depth_load_op = depth_load,
		.depth_store_op = depth_store,
		.stencil_load_op = stencil_load,
		.stencil_store_op = stencil_store,
		.read_only = read_only,
		.clear = clear,
	};
	m_data.has_depth_attachment = true;

	return resolved_handle;
}

RGTextureHandle RGPassBuilder::set_color_attachment(Uint32 slot, RGTextureHandle handle, AttachmentLoadOp load_op,
													AttachmentStoreOp store_op, ClearColor clear, Uint32 mip_level,
													Uint32 array_layer) {
	AQUILA_ASSERT(handle.is_valid(), "SetColorAttachment: invalid handle");

	// Color attachments are always writes, bump the version.
	RGTextureHandle new_handle = write_texture(handle, ResourceState::ColorAttachment);

	// Grow the slot array to fit.
	if (slot >= m_data.color_attachments.size()) {
		m_data.color_attachments.resize(slot + 1,
										RGColorAttachment{ .handle = RGTextureHandle{},
														   .load_op = AttachmentLoadOp::DontCare,
														   .store_op = AttachmentStoreOp::DontCare,
														   .clear = {},
														   .mip_level = 0,
														   .array_layer = 0 });
	}

	m_data.color_attachments[slot] = RGColorAttachment{
		.handle = new_handle,
		.load_op = load_op,
		.store_op = store_op,
		.clear = clear,
		.mip_level = mip_level,
		.array_layer = array_layer,
	};

	return new_handle;
}

// Duplicate access guards
//
// Catching these at setup time is cheaper than debugging a mis-ordered barrier
// at runtime.  We match on the slot index (lower 24 bits) so that a stale
// handle and a current handle to the same slot are still caught.

void RGPassBuilder::assert_no_duplicate_texture_access(RGTextureHandle handle, bool is_write) const {
	const Uint32 slot = slot_of(handle.id);

	auto slot_matches = [slot](const RGTextureAccess &a) { return slot_of(a.handle.id) == slot; };

	const bool in_reads = std::ranges::any_of(m_data.texture_reads, slot_matches);
	const bool in_writes = std::ranges::any_of(m_data.texture_writes, slot_matches);

	if (is_write) {
		AQUILA_ASSERT(!in_reads, "Texture slot is already declared as a read in this pass — cannot also write it");
		AQUILA_ASSERT(!in_writes, "Texture slot is already declared as a write in this pass — double-write detected");
	} else {
		// Duplicate reads are caught upstream in ReadTexture before reaching here.
		AQUILA_ASSERT(!in_reads, "Texture slot is already declared as a read in this pass");
		AQUILA_ASSERT(!in_writes, "Texture slot is already declared as a write in this pass — cannot also read it");
	}
}

void RGPassBuilder::assert_no_duplicate_buffer_access(RGBufferHandle handle, bool is_write) const {
	const Uint32 slot = slot_of(handle.id);

	auto slot_matches = [slot](const RGBufferAccess &a) { return slot_of(a.handle.id) == slot; };

	const bool in_reads = std::ranges::any_of(m_data.buffer_reads, slot_matches);
	const bool in_writes = std::ranges::any_of(m_data.buffer_writes, slot_matches);

	if (is_write) {
		AQUILA_ASSERT(!in_reads, "Buffer slot is already declared as a read in this pass — cannot also write it");
		AQUILA_ASSERT(!in_writes, "Buffer slot is already declared as a write in this pass — double-write detected");
	} else {
		AQUILA_ASSERT(!in_reads, "Buffer slot is already declared as a read in this pass");
		AQUILA_ASSERT(!in_writes, "Buffer slot is already declared as a write in this pass — cannot also read it");
	}
}

} // namespace Aquila::Graphics::RG
