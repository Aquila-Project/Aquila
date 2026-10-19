#ifndef TRANSFORM_COMPONENT_H
#define TRANSFORM_COMPONENT_H

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Signal.h"
#include "glm/gtc/quaternion.hpp"
#include "glm/trigonometric.hpp"
namespace Aquila::SceneManagement::Components {
/// Setters mark the transform dirty and tell the scene.
/// Scene::update_transform_hierarchy() recomputes the world matrices once per frame.
struct TransformComponent {
  public:
	Signal<void()> on_changed;

	TransformComponent(const Vec3 &position = Vec3{ 0.F },
					   const Quaternion &rotation = Quaternion{ 1.F, 0.F, 0.F, 0.F }, const Vec3 &scale = Vec3{ 1.F })
		: m_local_position(position), m_local_rotation(rotation), m_local_scale(scale), m_world_matrix(1.0F) {}

	void set_local_position(const Vec3 &position) {
		m_local_position = position;
		mark_world_matrix_dirty();
	}

	void set_local_rotation(const Quaternion &rotation) {
		m_local_rotation = rotation;
		mark_world_matrix_dirty();
	}

	/// Degrees.
	void set_local_rotation_euler(const Vec3 &rotation) {
		m_local_rotation = Quaternion(Math::radians(rotation));
		mark_world_matrix_dirty();
	}

	void set_local_scale(const Vec3 &scale) {
		m_local_scale = scale;
		mark_world_matrix_dirty();
	}

	[[nodiscard]] const Vec3 &get_local_position() const { return m_local_position; }
	[[nodiscard]] const Quaternion &get_local_rotation() const { return m_local_rotation; }
	/// Degrees.
	[[nodiscard]] Vec3 get_local_rotation_euler() const { return Math::degrees(glm::eulerAngles(m_local_rotation)); }
	[[nodiscard]] const Vec3 &get_local_scale() const { return m_local_scale; }

	// Mutable versions only when you need to modify directly
	Vec3 &get_local_position_mut() {
		mark_world_matrix_dirty();
		return m_local_position;
	}
	Quaternion &get_local_rotation_mut() {
		mark_world_matrix_dirty();
		return m_local_rotation;
	}
	Vec3 &get_local_scale_mut() {
		mark_world_matrix_dirty();
		return m_local_scale;
	}

	[[nodiscard]] Mat4 get_local_transform_matrix() const {
		Mat4 translation_matrix = glm::translate(Mat4(1.0F), m_local_position);
		Mat4 rotation_matrix = glm::toMat4(m_local_rotation);
		Mat4 scale_matrix = glm::scale(Mat4(1.0F), m_local_scale);
		return translation_matrix * rotation_matrix * scale_matrix;
	}

	void update_world_matrix(const Mat4 &parent_matrix = Mat4(1.0F)) {
		m_world_matrix = parent_matrix * get_local_transform_matrix();
		m_world_matrix_dirty = false;
	}

	// Get world matrix (const version - use cached value)
	[[nodiscard]] const Mat4 &get_world_matrix() const {
		AQUILA_ASSERT(!m_world_matrix_dirty, "World matrix is dirty! Call UpdateWorldMatrix() before rendering.");
		return m_world_matrix;
	}

	// Get world matrix (mutable version - lazy update)
	// WARNING: Only use this outside of render loops!
	const Mat4 &get_world_matrix_lazy() {
		if (m_world_matrix_dirty) {
			update_world_matrix(m_parent_matrix);
		}
		return m_world_matrix;
	}

	// Extract world position from world matrix
	[[nodiscard]] Vec3 get_world_position() const { return Vec3(m_world_matrix[3]); }

	// Extract world scale from world matrix
	[[nodiscard]] Vec3 get_world_scale() const {
		Vec3 scale;
		scale.x = Math::length(Vec3(m_world_matrix[0]));
		scale.y = Math::length(Vec3(m_world_matrix[1]));
		scale.z = Math::length(Vec3(m_world_matrix[2]));
		return scale;
	}

	// Extract world rotation from world matrix
	[[nodiscard]] Quaternion get_world_rotation() const {
		Vec3 scale = get_world_scale();

		// Create rotation matrix by removing scale
		Mat3 rotation_matrix;
		rotation_matrix[0] = Vec3(m_world_matrix[0]) / scale.x;
		rotation_matrix[1] = Vec3(m_world_matrix[1]) / scale.y;
		rotation_matrix[2] = Vec3(m_world_matrix[2]) / scale.z;

		return glm::quat_cast(rotation_matrix);
	}

	[[nodiscard]] Mat3 get_normal_matrix() const {
		Mat3 normal_matrix = Mat3(m_world_matrix);
		return Math::transpose(Math::inverse(normal_matrix));
	}

	/// Only right with uniform scale.
	[[nodiscard]] Mat3 get_normal_matrix_fast() const { return Mat3(m_world_matrix); }

	void set_parent_matrix(const Mat4 &parent_matrix) {
		m_parent_matrix = parent_matrix;
		mark_world_matrix_dirty();
	}

	[[nodiscard]] bool is_world_matrix_dirty() const { return m_world_matrix_dirty; }

	/// The scene sets this to track dirty transforms, don't replace it.
	void set_dirty_callback(std::function<void()> fn) { m_on_dirty = std::move(fn); }

  private:
	void mark_world_matrix_dirty() {
		m_world_matrix_dirty = true;
		if (m_on_dirty) {
			m_on_dirty();
		}
	}

	Vec3 m_local_position{ 0.F };
	Quaternion m_local_rotation{ 1.F, 0.F, 0.F, 0.F };
	Vec3 m_local_scale{ 1.F };
	Mat4 m_world_matrix{ 1.F };
	Mat4 m_parent_matrix{ 1.F };
	bool m_world_matrix_dirty{ true };
	std::function<void()> m_on_dirty;
};
} // namespace Aquila::SceneManagement::Components
#endif
