#ifndef CAMERA_COMPONENT_H
#define CAMERA_COMPONENT_H

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/Signal.h"

#include <glm/gtc/quaternion.hpp>

namespace Aquila::SceneManagement::Components {
using namespace glm;

/// `fov` is vertical, in degrees. +Z is forward and -Y is up (Vulkan y-down).
/// `primary` marks the camera find_primary_camera() returns.
struct CameraComponent {
	F32 fov = 80.0F;
	F32 near_plane = 0.1F;
	F32 far_plane = 100.F;
	F32 aspect_ratio = 1.778F;
	bool is_orthographic = false;

	F32 ortho_left = -1.0F;
	F32 ortho_right = 1.0F;
	F32 ortho_top = 1.0F;
	F32 ortho_bottom = -1.0F;

	bool primary = false;

	Signal<void()> on_changed;

	Mat4 get_view_matrix(const Vec3 &position, const quat &rotation) const {
		const Vec3 forward = rotation * Vec3(0.0F, 0.0F, 1.0F);
		const Vec3 up = rotation * Vec3(0.0F, -1.0F, 0.0F);
		return Math::look_in_direction(position, forward, up);
	}

	F32 get_near_plane() const { return near_plane; }

	F32 get_far_plane() const { return far_plane; }

	/// The perspective one is y-flipped for Vulkan, the orthographic one isn't.
	Mat4 get_projection_matrix() const {
		if (is_orthographic) {
			return Math::ortho_vulkan(ortho_left, ortho_right, ortho_bottom, ortho_top, near_plane, far_plane);
		}
		Mat4 proj_matrix = Math::perspective_vulkan(Math::radians(fov), aspect_ratio, near_plane, far_plane);
		proj_matrix[1][1] *= -1.0F;
		return proj_matrix;
	}

	Vec3 get_forward_direction(const quat &rotation) const { return rotation * Vec3(0.0F, 0.0F, 1.0F); }

	Vec3 get_right_direction(const quat &rotation) const { return rotation * Vec3(1.0F, 0.0F, 0.0F); }

	Vec3 get_up_direction(const quat &rotation) const { return rotation * Vec3(0.0F, -1.0F, 0.0F); }

	void on_resize(F32 width, F32 height) { aspect_ratio = width / height; }

	void set_fov(F32 new_fov) { fov = Math::clamp(new_fov, 1.0F, 179.0F); }

	void zoom(F32 offset) { set_fov(fov - offset); }
};

} // namespace Aquila::SceneManagement::Components

#endif
