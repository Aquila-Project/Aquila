#ifndef AQUILA_MATH_H
#define AQUILA_MATH_H

#include "Aquila/Foundation/Math/MathTypes.h"
#include "Aquila/Foundation/PrimitiveTypes.h"

#include <numbers>

namespace Aquila::Math {

constexpr F32 PI = std::numbers::pi_v<float>;
constexpr F32 TAU = 2.0F * PI;
constexpr F32 HALF_PI = PI / 2.0F;
constexpr F32 EPSILON = 1e-6F;
constexpr F32 DEG_TO_RAD = PI / 180.0F;
constexpr F32 RAD_TO_DEG = 180.0F / PI;

// Scalar / generic

template <typename T, typename U> AQUILA_FORCE_INLINE T clamp(T value, U min, U max) {
	return glm::clamp(value, T(min), T(max));
}

template <typename T> AQUILA_FORCE_INLINE T lerp(const T a, const T b, const F32 t) {
	return glm::mix(a, b, t);
}

AQUILA_FORCE_INLINE F32 smooth_step(const F32 edge0, const F32 edge1, const F32 x) {
	return glm::smoothstep(edge0, edge1, x);
}

AQUILA_FORCE_INLINE bool approx_equal(const F32 a, const F32 b, const F32 epsilon = EPSILON) {
	return std::abs(a - b) < epsilon;
}

template <typename T> AQUILA_FORCE_INLINE T radians(const T &degrees) {
	return glm::radians(degrees);
}

template <typename T> AQUILA_FORCE_INLINE T degrees(const T &radians) {
	return glm::degrees(radians);
}

template <typename T> AQUILA_FORCE_INLINE T sin(const T &angle) {
	return glm::sin(angle);
}

template <typename T> AQUILA_FORCE_INLINE T cos(const T &angle) {
	return glm::cos(angle);
}

template <typename T> AQUILA_FORCE_INLINE T tan(const T &angle) {
	return glm::tan(angle);
}

AQUILA_FORCE_INLINE F32 atan2(const F32 y, const F32 x) {
	return std::atan2(y, x);
}

AQUILA_FORCE_INLINE F32 mod(const F32 a, const F32 b) {
	return glm::mod(a, b);
}

AQUILA_FORCE_INLINE F32 normalize_angle(F32 angle) {
	angle = mod(angle, TAU);
	if (angle < 0.0F) {
		angle += TAU;
	}
	return angle;
}

AQUILA_FORCE_INLINE F32 angle_difference(const F32 a, const F32 b) {
	const F32 diff = mod(b - a + PI, TAU) - PI;
	return diff < -PI ? diff + TAU : diff;
}

// Component-wise vector ops

template <typename T> AQUILA_FORCE_INLINE T min(const T &a, const T &b) {
	return glm::min(a, b);
}

template <typename T> AQUILA_FORCE_INLINE T max(const T &a, const T &b) {
	return glm::max(a, b);
}

template <typename T> AQUILA_FORCE_INLINE T abs(const T &v) {
	return glm::abs(v);
}

template <typename T> AQUILA_FORCE_INLINE T floor(const T &v) {
	return glm::floor(v);
}

template <typename T> AQUILA_FORCE_INLINE T ceil(const T &v) {
	return glm::ceil(v);
}

template <typename T> AQUILA_FORCE_INLINE T round(const T &v) {
	return glm::round(v);
}

template <typename T> AQUILA_FORCE_INLINE T sign(const T &v) {
	return glm::sign(v);
}

template <typename T> AQUILA_FORCE_INLINE T fract(const T &v) {
	return glm::fract(v);
}

template <typename T> AQUILA_FORCE_INLINE T sqrt(const T &v) {
	return glm::sqrt(v);
}

template <typename T> AQUILA_FORCE_INLINE T pow(const T &base, const T &exp) {
	return glm::pow(base, exp);
}

// Vector ops

AQUILA_FORCE_INLINE F32 dot(const Vec2 &a, const Vec2 &b) {
	return glm::dot(a, b);
}

AQUILA_FORCE_INLINE F32 dot(const Vec3 &a, const Vec3 &b) {
	return glm::dot(a, b);
}

AQUILA_FORCE_INLINE F32 dot(const Vec4 &a, const Vec4 &b) {
	return glm::dot(a, b);
}

AQUILA_FORCE_INLINE Vec3 cross(const Vec3 &a, const Vec3 &b) {
	return glm::cross(a, b);
}

AQUILA_FORCE_INLINE F32 cross(const Vec2 &a, const Vec2 &b) {
	return (a.x * b.y) - (a.y * b.x);
}

template <typename T> AQUILA_FORCE_INLINE F32 length(const T &v) {
	return glm::length(v);
}

template <typename T> AQUILA_FORCE_INLINE F32 length_sq(const T &v) {
	return glm::dot(v, v);
}

template <typename T> AQUILA_FORCE_INLINE T normalize(const T &v) {
	return glm::normalize(v);
}

template <typename T> AQUILA_FORCE_INLINE F32 distance(const T &a, const T &b) {
	return glm::distance(a, b);
}

template <typename T> AQUILA_FORCE_INLINE T reflect(const T &v, const T &normal) {
	return glm::reflect(v, normal);
}

template <typename T> AQUILA_FORCE_INLINE T refract(const T &v, const T &normal, F32 eta) {
	return glm::refract(v, normal, eta);
}

// Matrix ops

template <typename T> AQUILA_FORCE_INLINE T inverse(const T &m) {
	return glm::inverse(m);
}

template <typename T> AQUILA_FORCE_INLINE T transpose(const T &m) {
	return glm::transpose(m);
}

AQUILA_FORCE_INLINE Quaternion angle_axis(const F32 angle, const Vec3 &axis) {
	return glm::angleAxis(angle, axis);
}

// Projection matrices (Vulkan)

inline Mat4 ortho_vulkan(F32 left, F32 right, F32 bottom, F32 top, F32 z_near, F32 z_far) {
	Mat4 result(1.0F);
	result[0][0] = 2.0F / (right - left);
	result[1][1] = 2.0F / (top - bottom);
	result[2][2] = 1.0F / (z_far - z_near);
	result[3][0] = -(right + left) / (right - left);
	result[3][1] = -(top + bottom) / (top - bottom);
	result[3][2] = -z_near / (z_far - z_near);
	return result;
}

inline Mat4 perspective_vulkan(const F32 fov_y, const F32 aspect, const F32 z_near, const F32 z_far) {
	const F32 tan_half_fovy = std::tan(fov_y / 2.0F);
	Mat4 result(0.0F);
	result[0][0] = 1.0F / (aspect * tan_half_fovy);
	result[1][1] = 1.0F / tan_half_fovy;
	result[2][2] = z_far / (z_far - z_near);
	result[2][3] = 1.0F;
	result[3][2] = -(z_far * z_near) / (z_far - z_near);
	return result;
}

inline Mat4 infinite_perspective_vulkan(const F32 fov_y, const F32 aspect, const F32 z_near) {
	const F32 tan_half_fovy = std::tan(fov_y / 2.0F);
	Mat4 result(0.0F);
	result[0][0] = 1.0F / (aspect * tan_half_fovy);
	result[1][1] = 1.0F / tan_half_fovy;
	result[2][2] = 1.0F;
	result[2][3] = 1.0F;
	result[3][2] = -z_near;
	return result;
}

// View matrices

inline Mat4 view_from_basis(const Vec3 &u, const Vec3 &v, const Vec3 &w, const Vec3 &position) {
	Mat4 result(1.0F);
	result[0][0] = u.x;
	result[1][0] = u.y;
	result[2][0] = u.z;
	result[0][1] = v.x;
	result[1][1] = v.y;
	result[2][1] = v.z;
	result[0][2] = w.x;
	result[1][2] = w.y;
	result[2][2] = w.z;
	result[3][0] = -dot(u, position);
	result[3][1] = -dot(v, position);
	result[3][2] = -dot(w, position);
	return result;
}

inline Mat4 inverse_view(const Mat4 &view, const Vec3 &position) {
	Mat4 result(transpose(Mat3(view)));
	result[3] = Vec4(position, 1.0F);
	return result;
}

inline Mat4 look_at(const Vec3 &position, const Vec3 &target, const Vec3 &up) {
	const Vec3 w = normalize(target - position);
	const Vec3 u = normalize(cross(w, up));
	const Vec3 v = cross(w, u);

	return view_from_basis(u, v, w, position);
}

inline Mat4 look_in_direction(const Vec3 &position, const Vec3 &direction, const Vec3 &world_up = Vec3(0, -1, 0)) {
	const Vec3 w = normalize(direction);
	const Vec3 u = normalize(cross(w, world_up));
	const Vec3 v = cross(w, u);

	return view_from_basis(u, v, w, position);
}

inline Mat4 view_from_euler(const Vec3 &position, const Vec3 &rotation) {
	const F32 c3 = std::cos(rotation.z);
	const F32 s3 = std::sin(rotation.z);
	const F32 c2 = std::cos(rotation.x);
	const F32 s2 = std::sin(rotation.x);
	const F32 c1 = std::cos(rotation.y);
	const F32 s1 = std::sin(rotation.y);

	const Vec3 u = { (c1 * c3) + (s1 * s2 * s3), c2 * s3, (c1 * s2 * s3) - (c3 * s1) };
	const Vec3 v = { (c3 * s1 * s2) - (c1 * s3), c2 * c3, (c1 * c3 * s2) + (s1 * s3) };
	const Vec3 w = { c2 * s1, -s2, c1 * c2 };

	return view_from_basis(u, v, w, position);
}

// Geometry utilities

inline std::array<Vec4, 8> extract_frustum_corners(const Mat4 &proj_view) {
	const Mat4 inv = inverse(proj_view);
	std::array<Vec4, 8> corners{};
	int idx = 0;
	for (int z = 0; z < 2; ++z) {
		for (int y = 0; y < 2; ++y) {
			for (int x = 0; x < 2; ++x) {
				Vec4 pt = inv * Vec4((x != 0) ? 1.F : -1.F, (y != 0) ? 1.F : -1.F, (z != 0) ? 1.F : 0.F, 1.F);
				corners[idx++] = pt / pt.w;
			}
		}
	}
	return corners;
}

inline void compute_aabb(const std::array<Vec3, 8> &points, Vec3 &out_min, Vec3 &out_max) {
	out_min = Vec3(std::numeric_limits<F32>::max());
	out_max = Vec3(std::numeric_limits<F32>::lowest());
	for (const auto &p : points) {
		out_min = min(out_min, p);
		out_max = max(out_max, p);
	}
}

inline void compute_light_space_aabb(const std::array<Vec4, 8> &corners, const Mat4 &light_view, Vec3 &out_min,
									 Vec3 &out_max) {
	out_min = Vec3(std::numeric_limits<F32>::max());
	out_max = Vec3(std::numeric_limits<F32>::lowest());
	for (const auto &corner : corners) {
		const Vec4 ls = light_view * corner;
		const Vec3 lsp = Vec3(ls.x, ls.y, ls.z);
		out_min = min(out_min, lsp);
		out_max = max(out_max, lsp);
	}
}

inline Mat4 build_light_view_matrix(const Vec3 &light_direction, const Vec3 &focus_point, const F32 distance = 100.0F) {
	const Vec3 light_dir = normalize(light_direction);
	Vec3 world_up = Vec3(0.F, 1.F, 0.F);
	if (std::abs(dot(light_dir, world_up)) > 0.99F) {
		world_up = Vec3(0.F, 0.F, 1.F);
	}

	const Vec3 w = light_dir;
	const Vec3 u = normalize(cross(w, world_up));
	const Vec3 v = cross(w, u);
	const Vec3 light_pos = focus_point - light_dir * distance;

	return view_from_basis(u, v, w, light_pos);
}

inline void snap_to_texel_grid(F32 &min, F32 &max, const F32 world_units_per_texel) {
	min = std::floor(min / world_units_per_texel) * world_units_per_texel;
	max = std::floor(max / world_units_per_texel) * world_units_per_texel;
}

inline F32 fix_z_bounds_for_shadows(const F32 min_z, const F32 max_z, F32 &out_near, F32 &out_far) {
	F32 z_translation = 0.0F;
	if (min_z < 0.0F) {
		z_translation = -min_z;
		out_near = 0.0F;
		out_far = max_z - min_z;
	} else {
		out_near = min_z;
		out_far = max_z;
	}
	if (out_far <= out_near) {
		out_far = out_near + 1.0F;
	}
	return z_translation;
}

AQUILA_FORCE_INLINE F32 distance_to_plane(const Vec3 &point, const Vec3 &normal, const Vec3 &plane_point) {
	return dot(normal, point - plane_point);
}

AQUILA_FORCE_INLINE Vec3 project_onto_plane(const Vec3 &point, const Vec3 &normal, const Vec3 &plane_point) {
	return point - normal * distance_to_plane(point, normal, plane_point);
}

AQUILA_FORCE_INLINE F32 distance_to_segment(const Vec2 &point, const Vec2 &a, const Vec2 &b) {
	const Vec2 ab = b - a;
	const F32 length_squared = dot(ab, ab);
	const F32 t = length_squared > 0.F ? clamp(dot(point - a, ab) / length_squared, 0.F, 1.F) : 0.F;
	return length(point - (a + (ab * t)));
}

AQUILA_FORCE_INLINE Vec3 barycentric(const Vec3 &p, const Vec3 &a, const Vec3 &b, const Vec3 &c) {
	const Vec3 v0 = b - a, v1 = c - a, v2 = p - a;
	const F32 d00 = dot(v0, v0), d01 = dot(v0, v1), d11 = dot(v1, v1);
	const F32 d20 = dot(v2, v0), d21 = dot(v2, v1);
	const F32 denom = d00 * d11 - d01 * d01;
	if (std::abs(denom) < EPSILON) {
		return Vec3(0.F);
	}
	const F32 vv = (d11 * d20 - d01 * d21) / denom;
	const F32 ww = (d00 * d21 - d01 * d20) / denom;
	return Vec3(1.0F - vv - ww, vv, ww);
}

AQUILA_FORCE_INLINE F32 signed_volume(const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec3 &d) {
	return dot(cross(b - a, c - a), d - a) / 6.0F;
}

} // namespace Aquila::Math

#endif // AQUILA_MATH_H
