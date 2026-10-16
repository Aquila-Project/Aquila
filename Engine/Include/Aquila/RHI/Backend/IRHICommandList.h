#ifndef AQUILA_IRHI_COMMAND_LIST_H
#define AQUILA_IRHI_COMMAND_LIST_H

#include "Aquila/RHI/Backend/RHITypes.h"
#include "Aquila/RHI/Backend/IRHIBuffer.h"
#include "Aquila/RHI/Backend/IRHITexture.h"
#include "Aquila/RHI/Backend/IRHIDescriptors.h"
#include "Aquila/RHI/Backend/IRHIPipeline.h"
#include "Aquila/RHI/Backend/IRHIQueryPool.h"

namespace Aquila::RHI {

class IRHICommandList {
  public:
	virtual ~IRHICommandList() = default;

	IRHICommandList(const IRHICommandList &) = delete;
	IRHICommandList &operator=(const IRHICommandList &) = delete;

	virtual void begin() = 0;
	virtual void end() = 0;
	virtual void reset() = 0;

	[[nodiscard]] virtual bool is_recording() const = 0;
	[[nodiscard]] virtual CommandListType get_type() const = 0;
	[[nodiscard]] virtual const std::string &get_name() const = 0;

	virtual void transition_texture(IRHITexture &texture, ResourceState old_state, ResourceState new_state) = 0;
	virtual void transition_buffer(IRHIBuffer &buffer, ResourceState old_state, ResourceState new_state) = 0;

	// Binding a pipeline also captures the pipeline layout, which is required for
	// subsequent BindDescriptorSet and PushConstants calls.

	virtual void bind_pipeline(IRHIPipeline &pipeline) = 0;
	virtual void set_viewport(float x, float y, float width, float height, float min_depth = 0.0F,
							  float max_depth = 1.0F) = 0;
	virtual void set_scissor(Int32 x, Int32 y, Uint32 width, Uint32 height) = 0;

	virtual void bind_descriptor_set(Uint32 set, IRHIDescriptorSet &descriptor_set) = 0;
	virtual void push_constants(const void *data, Uint32 size, ShaderStageFlags stages, Uint32 offset = 0) = 0;
	virtual void bind_vertex_buffer(IRHIBuffer &buffer, Uint32 binding = 0, Uint64 offset = 0) = 0;
	virtual void bind_index_buffer(IRHIBuffer &buffer, IndexFormat format = IndexFormat::UInt32, Uint64 offset = 0) = 0;

	virtual void draw(Uint32 vertex_count, Uint32 instance_count = 1, Uint32 first_vertex = 0,
					  Uint32 first_instance = 0) = 0;
	virtual void draw_indexed(Uint32 index_count, Uint32 instance_count = 1, Uint32 first_index = 0,
							  Int32 vertex_offset = 0, Uint32 first_instance = 0) = 0;
	virtual void draw_indirect(IRHIBuffer &buffer, Uint64 offset, Uint32 draw_count, Uint32 stride) = 0;
	virtual void draw_indexed_indirect(IRHIBuffer &buffer, Uint64 offset, Uint32 draw_count, Uint32 stride) = 0;

	virtual void copy_buffer_to_texture(IRHIBuffer &src, IRHITexture &dst, Uint32 width, Uint32 height,
										Uint32 dst_array_layer = 0, Uint32 dst_mip_level = 0) = 0;
	virtual void copy_texture_to_buffer(IRHITexture &src, IRHIBuffer &dst, Uint32 width, Uint32 height,
										Uint32 src_array_layer = 0, Uint32 src_mip_level = 0, Int32 src_offset_x = 0,
										Int32 src_offset_y = 0) = 0;
	virtual void fill_buffer(IRHIBuffer &buffer, Uint64 offset, Uint64 size, Uint32 value) = 0;

	/// Group counts, not threads.
	virtual void dispatch(Uint32 x, Uint32 y, Uint32 z) = 0;

	virtual void reset_queries(IRHIQueryPool &pool, Uint32 first, Uint32 count) = 0;
	virtual void write_timestamp(IRHIQueryPool &pool, Uint32 index) = 0;

	virtual void push_debug_group(const char *name) = 0;
	virtual void pop_debug_group() = 0;

  protected:
	IRHICommandList() = default;
};

} // namespace Aquila::RHI
#endif
