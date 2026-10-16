#pragma once
#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/RHI/Backend/IRHIBuffer.h"

namespace Aquila::GFX {

class GfxContext;

class GfxBuffer {
  public:
	~GfxBuffer() = default;
	AQUILA_NONCOPYABLE(GfxBuffer);

	/// Mapped buffers only (every domain except GpuOnly). size 0 copies the whole buffer.
	void write(const void *data, Uint64 size = 0, Uint64 offset = 0);
	void *map();
	void unmap();
	void flush(Uint64 size = 0, Uint64 offset = 0);
	void invalidate(Uint64 size = 0, Uint64 offset = 0);

	void destroy_immediate();

	[[nodiscard]] Uint64 get_size() const;
	[[nodiscard]] Uint32 get_instance_count() const;
	[[nodiscard]] bool is_mapped() const;
	[[nodiscard]] RHI::IRHIBuffer &get_rhi() { return *m_buffer; }

  private:
	friend class GfxContext;
	explicit GfxBuffer(Unique<RHI::IRHIBuffer> buffer);
	Unique<RHI::IRHIBuffer> m_buffer;
};

} // namespace Aquila::GFX
