#ifndef AQUILA_IRHI_SWAPCHAIN_H
#define AQUILA_IRHI_SWAPCHAIN_H

#include "Aquila/RHI/Backend/RHITypes.h"

namespace Aquila::RHI {

/// Only the main window passes drive_device_frame = true to acquire_next_image().
/// That also recycles the device's per-frame resources (deferred deletions, frame command pool),
/// which must happen once per frame, not once per window.
class IRHISwapchain {
  public:
	virtual ~IRHISwapchain() = default;

	IRHISwapchain(const IRHISwapchain &) = delete;
	IRHISwapchain &operator=(const IRHISwapchain &) = delete;

	// Returns false when the swapchain is out-of-date (must call Resize before next frame).
	virtual bool acquire_next_image(Uint32 &out_image_index, bool drive_device_frame) = 0;

	[[nodiscard]] virtual Uint32 get_width() const = 0;
	[[nodiscard]] virtual Uint32 get_height() const = 0;
	[[nodiscard]] virtual TextureFormat get_format() const = 0;
	[[nodiscard]] virtual Uint32 get_image_count() const = 0;

	// True after a suboptimal present; also call Resize to fix it.
	[[nodiscard]] virtual bool needs_resize() const = 0;
	virtual void resize(Uint32 width, Uint32 height) = 0;

	[[nodiscard]] virtual Uint32 get_current_frame_slot() const = 0;

  protected:
	IRHISwapchain() = default;
};

} // namespace Aquila::RHI
#endif
