#ifndef AQUILA_IRHI_BUFFER_H
#define AQUILA_IRHI_BUFFER_H

#include "Aquila/RHI/Backend/RHITypes.h"

namespace Aquila::RHI {

/// Destroying it is deferred until the frame slot comes back, so dropping one mid-frame is fine.
class IRHIBuffer {
  public:
	virtual ~IRHIBuffer() = default;

	IRHIBuffer(const IRHIBuffer &) = delete;
	IRHIBuffer &operator=(const IRHIBuffer &) = delete;

	/// Mapped buffers only (every domain except GpuOnly). size 0 copies the whole buffer.
	virtual void write(const void *data, Uint64 size, Uint64 offset = 0) = 0;
	virtual void *map() = 0;
	virtual void unmap() = 0;
	/// size 0 means the whole buffer. Only matters for non-coherent memory.
	virtual void flush(Uint64 size = 0, Uint64 offset = 0) = 0;
	virtual void invalidate(Uint64 size = 0, Uint64 offset = 0) = 0;

	/// Skips the deferred deletion. The GPU must be done with it.
	virtual void destroy_immediate() = 0;

	[[nodiscard]] virtual Uint64 get_size() const = 0;
	[[nodiscard]] virtual Uint32 get_instance_count() const = 0;
	[[nodiscard]] virtual bool is_mapped() const = 0;

  protected:
	IRHIBuffer() = default;
};

} // namespace Aquila::RHI
#endif
