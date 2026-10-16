#ifndef AQUILA_IRHI_TEXTURE_H
#define AQUILA_IRHI_TEXTURE_H

#include "Aquila/RHI/Backend/RHITypes.h"

namespace Aquila::RHI {

/// Destroying it is deferred until the frame slot comes back, so dropping one mid-frame is fine.
class IRHITexture {
  public:
	virtual ~IRHITexture() = default;

	IRHITexture(const IRHITexture &) = delete;
	IRHITexture &operator=(const IRHITexture &) = delete;

	/// Skips the deferred deletion. The GPU must be done with it.
	virtual void destroy_immediate() = 0;

	// Returns false if the underlying image view is not yet valid (e.g. lazy init or after DestroyImmediate).
	[[nodiscard]] virtual bool is_ready() const = 0;

	[[nodiscard]] virtual Uint32 get_width() const = 0;
	[[nodiscard]] virtual Uint32 get_height() const = 0;
	[[nodiscard]] virtual Uint32 get_mip_levels() const = 0;
	[[nodiscard]] virtual Uint32 get_array_layers() const = 0;
	[[nodiscard]] virtual TextureFormat get_format() const = 0;
	[[nodiscard]] virtual SampleCount get_sample_count() const = 0;
	[[nodiscard]] virtual const TextureDesc &get_desc() const = 0;

  protected:
	IRHITexture() = default;
};

} // namespace Aquila::RHI
#endif
