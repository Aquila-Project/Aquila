#ifndef AQUILA_RENDERING_VIEW_MATH_H
#define AQUILA_RENDERING_VIEW_MATH_H

#include "Aquila/Foundation/Math/Geometry/Ray.h"
#include "Aquila/Foundation/Math/Rect.h"
#include "Aquila/Rendering/RenderView.h"

namespace Aquila::Rendering {

/// World point to screen pixels inside `viewport`. nullopt when it's behind the camera.
[[nodiscard]] inline Option<Vec2> project_to_screen(const RenderView &view, const Rect &viewport, const Vec3 &world) {
	const Vec4 clip = view.projection * view.view * Vec4(world, 1.F);
	if (clip.w <= 1e-5F) {
		return std::nullopt;
	}
	const Vec2 ndc = Vec2(clip.x, clip.y) / clip.w;
	return viewport.position + (((ndc * 0.5F) + Vec2(0.5F)) * viewport.size);
}

/// The inverse of project_to_screen(): the ray under a screen pixel, for picking and gizmos.
[[nodiscard]] inline Math::Geometry::Ray::Ray screen_ray(const RenderView &view, const Rect &viewport, Vec2 screen) {
	const Vec2 ndc = (((screen - viewport.position) / viewport.size) * 2.F) - Vec2(1.F);
	const Mat4 inverse_view_projection = Math::inverse(view.projection * view.view);
	Vec4 near_point = inverse_view_projection * Vec4(ndc, 0.F, 1.F);
	Vec4 far_point = inverse_view_projection * Vec4(ndc, 1.F, 1.F);
	near_point /= near_point.w;
	far_point /= far_point.w;
	return { Vec3(near_point), Vec3(far_point) - Vec3(near_point) };
}

/// How big one pixel is at that point's depth. Gizmos use it to keep a constant size on screen.
[[nodiscard]] inline F32 world_units_per_pixel(const RenderView &view, const Rect &viewport, const Vec3 &world) {
	const F32 distance = Math::max(Math::dot(world - view.position, view.forward), view.near_plane);
	const F32 tan_half_fov = 1.F / Math::abs(view.projection[1][1]);
	return (2.F * distance * tan_half_fov) / Math::max(viewport.size.y, 1.F);
}

} // namespace Aquila::Rendering

#endif
