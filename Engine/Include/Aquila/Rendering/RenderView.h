#pragma once
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/Photometry.h"

namespace Aquila::Rendering {

/// The camera a frame renders from. `fov` is vertical, in degrees. `ev100` is the exposure when `auto_exposure` is
/// off, `exposure_compensation` (stops, + is brighter) shifts it when it's on. `bloom`/`bloom_intensity` are the
/// camera's emissive glow (BloomSystem). `valid` false means there's no camera.
struct RenderView {
	Mat4 view = Mat4(1.F);
	Mat4 projection = Mat4(1.F);
	Vec3 position = Vec3(0.F);
	Vec3 forward = Vec3(0.F, 0.F, 1.F);
	Vec3 up = Vec3(0.F, 1.F, 0.F);
	Vec3 right = Vec3(1.F, 0.F, 0.F);
	F32 near_plane = 0.1F;
	F32 far_plane = 1000.F;
	F32 fov = 60.F;
	F32 aspect = 1.F;
	F32 ev100 = Foundation::Photometry::DAYLIGHT_EV100;
	F32 exposure_compensation = 0.F;
	bool auto_exposure = true;
	bool bloom = true;
	F32 bloom_intensity = 1.F;
	bool is_orthographic = false;
	bool valid = false;

	bool operator==(const RenderView &) const = default;
};

} // namespace Aquila::Rendering
