#pragma once
#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/RHI/Backend/IRHISwapchain.h"

namespace Aquila::GFX {

class GfxContext;

class GfxSwapchain {
  public:
	~GfxSwapchain() = default;
	AQUILA_NONCOPYABLE(GfxSwapchain);

	/// Secondary windows pass false for drive_device_frame, see IRHISwapchain.
	/// @return false when the swapchain needs a resize, skip the frame
	bool acquire_next_image(Uint32 &out_image_index, bool drive_device_frame = true);

	[[nodiscard]] Uint32 get_width() const;
	[[nodiscard]] Uint32 get_height() const;
	[[nodiscard]] RHI::TextureFormat get_format() const;
	[[nodiscard]] Uint32 get_image_count() const;
	[[nodiscard]] bool needs_resize() const;
	void resize(Uint32 width, Uint32 height);
	[[nodiscard]] Uint32 get_current_frame_slot() const;
	[[nodiscard]] RHI::IRHISwapchain &get_rhi() { return *m_swapchain; }

  private:
	friend class GfxContext;
	explicit GfxSwapchain(Unique<RHI::IRHISwapchain> swapchain);
	Unique<RHI::IRHISwapchain> m_swapchain;
};

} // namespace Aquila::GFX
