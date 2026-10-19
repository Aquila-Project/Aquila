#pragma once
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/SharedConstants.h"

namespace Aquila::Rendering {

struct alignas(16) GpuCameraData {
	Mat4 view;
	Mat4 projection;
	Mat4 view_projection;
	Mat4 inverse_view;
	Mat4 inverse_projection;
	Mat4 inverse_view_projection;
	Mat4 prev_view_projection;

	Vec4 position;
	Vec4 forward;
	Vec4 up;
	Vec4 right;

	float near_plane;
	float far_plane;
	float fov;
	float aspect_ratio;

	Vec2 resolution;
	Vec2 jitter;

	Uint32 is_orthographic;
	Uint32 camera_index;
	Uint32 pad[2];
};

struct GpuFrameData {
	GpuCameraData main_camera;
	GpuCameraData cameras[SharedConstants::MAX_CAMERAS];

	Uint32 camera_count;
	float time;
	float delta_time;
	Uint32 frame_index;

	Vec2 screen_resolution;
	Uint32 light_count;
	/// Pixel value per nit from the main camera's EV100. Lit shaders multiply their output by it.
	float exposure;
};

} // namespace Aquila::Rendering
