#include "UI/Viewport/TransformGizmo.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Platform/Input.h"
#include "Aquila/Rendering/ViewMath.h"
#include "Aquila/Scene/Components/TransformComponent.h"
#include "Aquila/UI/Text/FontAtlas.h"
#include "Core/EditorContext.h"

#include <cstdio>

namespace Editor {

using namespace Aquila;
using SceneManagement::Entity;
using SceneManagement::Components::TransformComponent;

namespace {

constexpr F32 k_handle_px = 96.F;
constexpr F32 k_pick_radius_px = 7.F;
constexpr F32 k_center_radius_px = 9.F;
constexpr F32 k_plane_offset = 0.3F;
constexpr F32 k_plane_half_px = 7.F;
constexpr F32 k_min_axis_px = 14.F;
constexpr int k_ring_segments = 64;

constexpr std::array<Vec4, 3> k_axis_colors = {
	Vec4(0.86F, 0.36F, 0.36F, 1.F),
	Vec4(0.42F, 0.74F, 0.40F, 1.F),
	Vec4(0.33F, 0.55F, 0.90F, 1.F),
};
constexpr Vec4 k_center_color = Vec4(0.92F, 0.92F, 0.92F, 1.F);
constexpr std::array<const char *, 3> k_axis_names = { "X", "Y", "Z" };

F32 snap_value(F32 value, F32 step) {
	return step > 0.F ? Math::round(value / step) * step : value;
}

Quaternion rotation_of(const Mat4 &matrix) {
	Mat3 basis(matrix);
	for (int i = 0; i < 3; ++i) {
		const F32 length = Math::length(basis[i]);
		if (length > Math::EPSILON) {
			basis[i] /= length;
		}
	}
	return glm::quat_cast(basis);
}

} // namespace

TransformGizmo::TransformGizmo(EditorContext &context) : ViewportCanvas(10), m_context(context) {
	add_class("transform-gizmo");
}

TransformGizmo::Handle TransformGizmo::axis_handle(Usize axis) {
	return static_cast<Handle>(static_cast<Usize>(Handle::X) + axis);
}

TransformGizmo::Handle TransformGizmo::plane_handle(Usize normal) {
	return static_cast<Handle>(static_cast<Usize>(Handle::PlaneYZ) + normal);
}

bool TransformGizmo::is_axis(Handle handle) {
	return handle >= Handle::X && handle <= Handle::Z;
}

bool TransformGizmo::is_plane(Handle handle) {
	return handle >= Handle::PlaneYZ && handle <= Handle::PlaneXY;
}

Usize TransformGizmo::axis_of(Handle handle) {
	return static_cast<Usize>(handle) - static_cast<Usize>(Handle::X);
}

Usize TransformGizmo::normal_of(Handle handle) {
	return static_cast<Usize>(handle) - static_cast<Usize>(Handle::PlaneYZ);
}

bool TransformGizmo::wants_local_axes(TransformTool tool) const {
	return tool == TransformTool::Scale || m_context.tools.get_space() == TransformSpace::Local;
}

TransformGizmo::Frame TransformGizmo::compute_frame(const Rect &viewport, const Rendering::RenderView &view,
													bool local_axes) const {
	Frame frame;
	frame.tool = m_context.tools.get_tool();
	frame.view = view;
	frame.viewport = viewport;

	Entity entity = m_context.selection.get();
	if (!view.valid || viewport.size.x < 1.F || viewport.size.y < 1.F || !m_context.selection.has() ||
		!entity.has_component<TransformComponent>()) {
		return frame;
	}

	auto &transform = entity.get_component<TransformComponent>();
	const Mat4 &world = transform.get_world_matrix_lazy();
	frame.origin = Vec3(world[3]);

	for (int i = 0; i < 3; ++i) {
		Vec3 axis(0.F);
		axis[i] = 1.F;
		if (local_axes) {
			const Vec3 column = Vec3(world[i]);
			const F32 length = Math::length(column);
			axis = length > Math::EPSILON ? column / length : axis;
		}
		frame.axes[static_cast<Usize>(i)] = axis;
	}

	const Option<Vec2> center = Rendering::project_to_screen(view, viewport, frame.origin);
	if (!center) {
		return frame;
	}
	frame.center = *center;
	frame.length = k_handle_px * Rendering::world_units_per_pixel(view, viewport, frame.origin);

	for (Usize i = 0; i < 3; ++i) {
		const Option<Vec2> tip =
			Rendering::project_to_screen(view, viewport, frame.origin + (frame.axes[i] * frame.length));
		frame.tips[i] = tip.value_or(frame.center);
		frame.axis_visible[i] = tip.has_value() && Math::length(frame.tips[i] - frame.center) > k_min_axis_px;
	}

	for (Usize normal = 0; normal < 3; ++normal) {
		const Vec3 a = frame.axes[(normal + 1) % 3];
		const Vec3 b = frame.axes[(normal + 2) % 3];
		const Vec3 point = frame.origin + ((a + b) * (frame.length * k_plane_offset));
		const Option<Vec2> projected = Rendering::project_to_screen(view, viewport, point);
		frame.plane_centers[normal] = projected.value_or(frame.center);
		const F32 facing = Math::abs(Math::dot(frame.axes[normal], view.forward));
		frame.plane_visible[normal] = projected.has_value() && facing > 0.2F;
	}

	for (Usize axis = 0; axis < 3; ++axis) {
		const Vec3 u = frame.axes[(axis + 1) % 3];
		const Vec3 v = frame.axes[(axis + 2) % 3];
		auto &points = frame.rings[axis];
		auto &front = frame.ring_front[axis];
		points.reserve(k_ring_segments + 1);
		front.reserve(k_ring_segments + 1);
		for (int s = 0; s <= k_ring_segments; ++s) {
			const F32 angle = (static_cast<F32>(s) / static_cast<F32>(k_ring_segments)) * Math::TAU;
			const Vec3 offset = ((u * Math::cos(angle)) + (v * Math::sin(angle))) * frame.length;
			const Option<Vec2> projected = Rendering::project_to_screen(view, viewport, frame.origin + offset);
			points.push_back(projected.value_or(frame.center));
			front.push_back(Math::dot(offset, view.forward) <= 0.F);
		}
	}

	frame.valid = true;
	return frame;
}

void TransformGizmo::sync(const Rect &viewport, const Rendering::RenderView &view) {
	fit(viewport);

	if (m_active && !m_drag.entity.exists()) {
		m_active = false;
	}

	const bool left_down = Platform::Input::is_mouse_button_pressed(Platform::MouseButton::Left);
	const bool right_down = Platform::Input::is_mouse_button_pressed(Platform::MouseButton::Right);
	const bool left_pressed = left_down && !m_left_was_down;
	const bool right_pressed = right_down && !m_right_was_down;
	m_left_was_down = left_down;
	m_right_was_down = right_down;

	const Vec2 mouse = Platform::Input::get_mouse_position();
	if (m_active && right_pressed) {
		cancel();
	} else if (m_active && m_drag.modal) {
		if (left_pressed) {
			confirm();
		} else if (mouse != m_drag.mouse) {
			update_drag(mouse);
		}
	}

	const bool local_axes = m_active ? m_drag.local_axes : wants_local_axes(m_context.tools.get_tool());
	Frame frame = compute_frame(viewport, view, local_axes);
	bool changed = frame != m_frame;
	m_frame = std::move(frame);

	const Handle hovered = hovered_at(mouse);
	changed = changed || hovered != m_hovered;
	m_hovered = hovered;

	const bool snap = m_active && snapping();
	changed = changed || snap != m_drawn_snapping;
	m_drawn_snapping = snap;

	if (changed) {
		redraw();
	}
}

TransformGizmo::Handle TransformGizmo::hovered_at(Vec2 point) const {
	if (m_active) {
		return m_drag.handle;
	}
	if (m_frame.valid && m_frame.viewport.contains(point)) {
		return pick(point);
	}
	return Handle::None;
}

TransformGizmo::Handle TransformGizmo::pick(Vec2 point) const {
	if (!m_frame.valid) {
		return Handle::None;
	}

	if (m_frame.tool == TransformTool::Rotate) {
		Handle best = Handle::None;
		F32 best_distance = k_pick_radius_px;
		for (Usize axis = 0; axis < 3; ++axis) {
			const auto &points = m_frame.rings[axis];
			const auto &front = m_frame.ring_front[axis];
			for (Usize s = 0; s + 1 < points.size(); ++s) {
				if (!front[s] && !front[s + 1]) {
					continue;
				}
				const F32 distance = Math::distance_to_segment(point, points[s], points[s + 1]);
				if (distance < best_distance) {
					best_distance = distance;
					best = axis_handle(axis);
				}
			}
		}
		return best;
	}

	if (Math::length(point - m_frame.center) < k_center_radius_px) {
		return Handle::Center;
	}

	if (m_frame.tool == TransformTool::Translate) {
		for (Usize plane = 0; plane < 3; ++plane) {
			const Vec2 delta = Math::abs(point - m_frame.plane_centers[plane]);
			if (m_frame.plane_visible[plane] && delta.x <= k_plane_half_px && delta.y <= k_plane_half_px) {
				return plane_handle(plane);
			}
		}
	}

	Handle best = Handle::None;
	F32 best_distance = k_pick_radius_px;
	for (Usize axis = 0; axis < 3; ++axis) {
		if (!m_frame.axis_visible[axis]) {
			continue;
		}
		const F32 distance = Math::distance_to_segment(point, m_frame.center, m_frame.tips[axis]);
		if (distance < best_distance) {
			best_distance = distance;
			best = axis_handle(axis);
		}
	}
	return best;
}

bool TransformGizmo::snapping() const {
	const bool control = Platform::Input::is_key_pressed(Platform::Events::KeyCode::LeftControl) ||
		Platform::Input::is_key_pressed(Platform::Events::KeyCode::RightControl);
	return m_context.tools.is_snapping() != control;
}

void TransformGizmo::constrain_to_axis(int axis) {
	if (axis < 0 || axis > 2 || (m_active && !m_drag.modal)) {
		return;
	}
	const Handle requested = axis_handle(static_cast<Usize>(axis));

	if (!m_active) {
		begin(requested, Platform::Input::get_mouse_position(), true, false);
		return;
	}

	Handle handle = requested;
	bool local = false;
	if (m_drag.handle == requested) {
		if (m_drag.local_axes) {
			handle = Handle::Center;
		} else {
			local = true;
		}
	}

	const Vec2 start_mouse = m_drag.start_mouse;
	const Vec2 mouse = m_drag.mouse;
	restore_start();
	m_active = false;
	if (begin(handle, start_mouse, true, local)) {
		update_drag(mouse);
	}
}

bool TransformGizmo::begin(Handle handle, Vec2 mouse, bool modal, bool local_axes) {
	Entity entity = m_context.selection.get();
	if (!m_context.selection.has() || !entity.has_component<TransformComponent>()) {
		return false;
	}

	Frame frame = compute_frame(m_frame.viewport, m_frame.view, local_axes);
	if (!frame.valid) {
		return false;
	}

	auto &transform = entity.get_component<TransformComponent>();
	m_drag = Drag{};
	m_drag.handle = handle;
	m_drag.tool = m_context.tools.get_tool();
	m_drag.modal = modal;
	m_drag.local_axes = local_axes;
	m_drag.entity = entity;
	m_drag.frame = std::move(frame);
	m_drag.start_mouse = mouse;
	m_drag.mouse = mouse;
	m_drag.start_position = transform.get_local_position();
	m_drag.start_rotation = transform.get_local_rotation();
	m_drag.start_scale = transform.get_local_scale();
	m_drag.parent_world = transform.get_world_matrix_lazy() * Math::inverse(transform.get_local_transform_matrix());

	const Frame &f = m_drag.frame;
	const auto ray = Rendering::screen_ray(f.view, f.viewport, mouse);
	if (m_drag.tool == TransformTool::Translate) {
		if (is_axis(handle)) {
			m_drag.start_param = ray.closest_param_on_line(f.origin, f.axes[axis_of(handle)]).value_or(0.F);
		} else if (is_plane(handle)) {
			m_drag.start_hit = ray.hit_plane(f.origin, f.axes[normal_of(handle)]).value_or(f.origin);
		} else {
			m_drag.start_hit = ray.hit_plane(f.origin, f.view.forward).value_or(f.origin);
		}
	}

	if (m_drag.tool == TransformTool::Rotate && !modal && is_axis(handle)) {
		const Usize axis = axis_of(handle);
		const auto &points = f.rings[axis];
		Usize nearest = 0;
		F32 best = std::numeric_limits<F32>::max();
		for (Usize s = 0; s < points.size(); ++s) {
			const F32 distance = Math::length(points[s] - mouse);
			if (distance < best) {
				best = distance;
				nearest = s;
			}
		}
		const F32 theta = (static_cast<F32>(nearest) / static_cast<F32>(k_ring_segments)) * Math::TAU;
		const Vec3 u = f.axes[(axis + 1) % 3];
		const Vec3 v = f.axes[(axis + 2) % 3];
		const Vec3 radial = (u * Math::cos(theta)) + (v * Math::sin(theta));
		const Vec3 tangent = (v * Math::cos(theta)) - (u * Math::sin(theta));
		const Vec3 point = f.origin + (radial * f.length);
		const Option<Vec2> a = Rendering::project_to_screen(f.view, f.viewport, point);
		const Option<Vec2> b = Rendering::project_to_screen(f.view, f.viewport, point + (tangent * f.length * 0.1F));
		Vec2 screen_tangent = a && b ? *b - *a : Vec2(0.F);
		if (Math::length(screen_tangent) < 1e-3F) {
			const Vec2 out = mouse - f.center;
			screen_tangent = Vec2(-out.y, out.x);
		}
		m_drag.ring_tangent = Math::normalize(screen_tangent);
		m_drag.ring_start_angle = theta;
	}

	const Vec2 from_center = mouse - f.center;
	m_drag.previous_angle = Math::atan2(from_center.y, from_center.x);
	m_active = true;
	m_hovered = handle;
	redraw();
	Foundation::FrameScheduler::get()->request_frame();
	return true;
}

void TransformGizmo::update_drag(Vec2 mouse) {
	if (!m_active || !m_drag.entity.exists()) {
		return;
	}
	m_drag.mouse = mouse;
	switch (m_drag.tool) {
	case TransformTool::Translate:
		apply_translate(mouse);
		break;
	case TransformTool::Rotate:
		apply_rotate(mouse);
		break;
	case TransformTool::Scale:
		apply_scale(mouse);
		break;
	}
	redraw();
}

void TransformGizmo::on_mouse_press(Platform::MouseButton btn, Vec2 pos) {
	View::on_mouse_press(btn, pos);
	if (btn != Platform::MouseButton::Left || m_active) {
		return;
	}
	const Handle handle = pick(pos);
	if (handle == Handle::None) {
		on_background_pressed(pos);
		return;
	}
	begin(handle, pos, false, wants_local_axes(m_context.tools.get_tool()));
}

void TransformGizmo::on_mouse_move(Vec2 pos) {
	if (m_active && !m_drag.modal && m_is_pressed) {
		update_drag(pos);
	}
}

void TransformGizmo::on_mouse_hover(Vec2 pos) {
	if (m_active && m_drag.modal) {
		update_drag(pos);
		return;
	}
	const Handle hovered = hovered_at(pos);
	if (hovered != m_hovered) {
		m_hovered = hovered;
		redraw();
	}
}

void TransformGizmo::on_mouse_release(Platform::MouseButton btn, Vec2 pos) {
	View::on_mouse_release(btn, pos);
	if (btn == Platform::MouseButton::Left && m_active && !m_drag.modal) {
		confirm();
	}
}

void TransformGizmo::confirm() {
	if (m_active) {
		finish();
	}
}

void TransformGizmo::cancel() {
	if (!m_active) {
		return;
	}
	restore_start();
	m_context.selection.on_entity_transformed(m_drag.entity);
	finish();
}

void TransformGizmo::restore_start() {
	if (!m_drag.entity.exists()) {
		return;
	}
	auto &transform = m_drag.entity.get_component<TransformComponent>();
	transform.set_local_position(m_drag.start_position);
	transform.set_local_rotation(m_drag.start_rotation);
	transform.set_local_scale(m_drag.start_scale);
}

void TransformGizmo::finish() {
	m_active = false;
	if (m_drag.entity.exists()) {
		m_context.selection.on_entity_modified(m_drag.entity);
	}
	redraw();
	Foundation::FrameScheduler::get()->request_frame();
}

void TransformGizmo::apply_translate(Vec2 mouse) {
	const Frame &f = m_drag.frame;
	const auto ray = Rendering::screen_ray(f.view, f.viewport, mouse);
	const bool snap = snapping();
	const F32 step = m_context.tools.get_translate_snap();
	Vec3 delta(0.F);

	if (is_axis(m_drag.handle)) {
		const Vec3 axis = f.axes[axis_of(m_drag.handle)];
		if (Math::abs(Math::dot(ray.direction, axis)) > 0.995F) {
			return;
		}
		const Option<F32> param = ray.closest_param_on_line(f.origin, axis);
		if (!param) {
			return;
		}
		const F32 distance = *param - m_drag.start_param;
		delta = axis * (snap ? snap_value(distance, step) : distance);
	} else if (is_plane(m_drag.handle)) {
		const Usize normal = normal_of(m_drag.handle);
		const Option<Vec3> hit = ray.hit_plane(f.origin, f.axes[normal]);
		if (!hit) {
			return;
		}
		delta = *hit - m_drag.start_hit;
		if (snap) {
			const Vec3 a = f.axes[(normal + 1) % 3];
			const Vec3 b = f.axes[(normal + 2) % 3];
			delta = (a * snap_value(Math::dot(delta, a), step)) + (b * snap_value(Math::dot(delta, b), step));
		}
	} else {
		const Option<Vec3> hit = ray.hit_plane(f.origin, f.view.forward);
		if (!hit) {
			return;
		}
		delta = *hit - m_drag.start_hit;
		if (snap) {
			delta = Vec3(snap_value(delta.x, step), snap_value(delta.y, step), snap_value(delta.z, step));
		}
	}

	m_drag.applied_offset = delta;
	const Vec3 local_delta = Vec3(Math::inverse(m_drag.parent_world) * Vec4(delta, 0.F));
	m_drag.entity.get_component<TransformComponent>().set_local_position(m_drag.start_position + local_delta);
	publish_change();
}

void TransformGizmo::apply_rotate(Vec2 mouse) {
	const Frame &f = m_drag.frame;
	const bool around_view = !is_axis(m_drag.handle);
	const Usize axis = around_view ? 0 : axis_of(m_drag.handle);
	const Vec3 world_axis = around_view ? -f.view.forward : f.axes[axis];

	F32 angle = 0.F;
	if (!m_drag.modal && !around_view) {
		angle = Math::dot(mouse - m_drag.start_mouse, m_drag.ring_tangent) / k_handle_px;
	} else {
		const Vec2 from_center = mouse - f.center;
		if (Math::length(from_center) < 2.F) {
			return;
		}
		const F32 current = Math::atan2(from_center.y, from_center.x);
		m_drag.total_angle += Math::angle_difference(m_drag.previous_angle, current);
		m_drag.previous_angle = current;

		const Vec3 u = around_view ? f.view.up : f.axes[(axis + 1) % 3];
		const Vec3 v = around_view ? f.view.right : f.axes[(axis + 2) % 3];
		const Option<Vec2> a = Rendering::project_to_screen(f.view, f.viewport, f.origin + (u * f.length));
		const Option<Vec2> b = Rendering::project_to_screen(
			f.view, f.viewport, f.origin + (((u * Math::cos(0.2F)) + (v * Math::sin(0.2F))) * f.length));
		F32 orientation = a && b ? Math::cross(*a - f.center, *b - f.center) : 0.F;
		if (Math::abs(orientation) < 1e-3F) {
			orientation = Math::dot(world_axis, f.view.forward) < 0.F ? -1.F : 1.F;
		}
		angle = orientation > 0.F ? m_drag.total_angle : -m_drag.total_angle;
	}

	if (snapping()) {
		angle = snap_value(angle, Math::radians(m_context.tools.get_rotate_snap_degrees()));
	}
	m_drag.applied_angle = angle;

	const Vec3 rotation_axis = around_view ? Math::normalize(Math::cross(f.view.up, f.view.right)) : world_axis;
	const Quaternion parent_rotation = rotation_of(m_drag.parent_world);
	const Vec3 parent_axis = Math::normalize(Math::inverse(parent_rotation) * rotation_axis);
	const Quaternion rotation = Math::angle_axis(angle, parent_axis) * m_drag.start_rotation;
	m_drag.entity.get_component<TransformComponent>().set_local_rotation(Math::normalize(rotation));
	publish_change();
}

void TransformGizmo::apply_scale(Vec2 mouse) {
	const Frame &f = m_drag.frame;
	const bool snap = snapping();
	const F32 step = m_context.tools.get_scale_snap();
	const bool uniform = !is_axis(m_drag.handle);
	const Usize axis = uniform ? 0 : axis_of(m_drag.handle);

	F32 factor = 1.F;
	if (m_drag.modal) {
		const F32 start = Math::max(Math::length(m_drag.start_mouse - f.center), 1.F);
		factor = Math::length(mouse - f.center) / start;
	} else if (uniform) {
		const Vec2 moved = mouse - m_drag.start_mouse;
		factor = 1.F + ((moved.x - moved.y) / k_handle_px);
	} else {
		const Vec2 handle = f.tips[axis] - f.center;
		const F32 handle_length = Math::length(handle);
		if (handle_length < 1.F) {
			return;
		}
		factor = 1.F + (Math::dot(mouse - m_drag.start_mouse, handle / handle_length) / handle_length);
	}
	factor = Math::max(factor, 0.001F);
	if (snap) {
		factor = Math::max(snap_value(factor, step), step);
	}

	Vec3 scale = m_drag.start_scale;
	Vec3 applied(1.F);
	for (int i = 0; i < 3; ++i) {
		if (uniform || static_cast<Usize>(i) == axis) {
			scale[i] *= factor;
			applied[i] = factor;
		}
	}
	m_drag.applied_scale = applied;
	m_drag.entity.get_component<TransformComponent>().set_local_scale(scale);
	publish_change();
}

void TransformGizmo::publish_change() {
	m_context.selection.on_entity_transformed(m_drag.entity);
	Foundation::FrameScheduler::get()->request_frame();
}

Vec4 TransformGizmo::color_for(Handle handle, Vec4 base) const {
	if (handle == m_hovered) {
		return { (Vec3(base) * 0.45F) + Vec3(0.55F), base.a };
	}
	return base;
}

std::string TransformGizmo::readout() const {
	const char *space = m_drag.local_axes ? "local" : "global";
	const bool on_axis = is_axis(m_drag.handle);
	const Usize axis = on_axis ? axis_of(m_drag.handle) : 0;
	const char *snap = snapping() ? "   snap" : "";
	std::array<char, 160> buffer{};

	switch (m_drag.tool) {
	case TransformTool::Translate:
		if (on_axis) {
			std::snprintf(buffer.data(), buffer.size(), "D: %.3f m  along %s %s%s",
						  Math::dot(m_drag.applied_offset, m_drag.frame.axes[axis]), space, k_axis_names[axis], snap);
		} else {
			std::snprintf(buffer.data(), buffer.size(), "D: %.3f m  %.3f m  %.3f m%s", m_drag.applied_offset.x,
						  m_drag.applied_offset.y, m_drag.applied_offset.z, snap);
		}
		break;
	case TransformTool::Rotate:
		if (on_axis) {
			std::snprintf(buffer.data(), buffer.size(), "Rot: %.1f°  around %s %s%s",
						  Math::degrees(m_drag.applied_angle), space, k_axis_names[axis], snap);
		} else {
			std::snprintf(buffer.data(), buffer.size(), "Rot: %.1f°  around view%s",
						  Math::degrees(m_drag.applied_angle), snap);
		}
		break;
	case TransformTool::Scale:
		if (on_axis) {
			std::snprintf(buffer.data(), buffer.size(), "Scale %s: %.3f%s", k_axis_names[axis],
						  m_drag.applied_scale[static_cast<int>(axis)], snap);
		} else {
			std::snprintf(buffer.data(), buffer.size(), "Scale: %.3f%s", m_drag.applied_scale.x, snap);
		}
		break;
	}
	return buffer.data();
}

void TransformGizmo::draw(UI::Rendering::DrawList &draw_list) const {
	if (!m_frame.valid) {
		return;
	}
	if (m_active) {
		draw_constraint(draw_list);
	}
	switch (m_frame.tool) {
	case TransformTool::Translate:
		draw_translate(draw_list);
		break;
	case TransformTool::Rotate:
		draw_rotate(draw_list);
		break;
	case TransformTool::Scale:
		draw_scale(draw_list);
		break;
	}
	if (m_active) {
		draw_readout(draw_list);
	}
}

void TransformGizmo::draw_constraint(UI::Rendering::DrawList &draw_list) const {
	if (!is_axis(m_drag.handle) || m_drag.tool == TransformTool::Rotate) {
		return;
	}
	const Frame &f = m_drag.frame;
	const Usize axis = axis_of(m_drag.handle);
	const Mat4 view_projection = f.view.projection * f.view.view;
	const Vec4 base = view_projection * Vec4(m_frame.origin, 1.F);
	if (base.w <= 1e-5F) {
		return;
	}
	const Vec4 toward = view_projection * Vec4(f.axes[axis], 0.F);
	const Vec2 half_size = f.viewport.size * 0.5F;
	const Vec2 center = f.viewport.position + (((Vec2(base) / base.w) + Vec2(1.F)) * half_size);
	const Vec2 tangent = ((Vec2(toward) * base.w) - (Vec2(base) * toward.w)) * half_size;
	if (Math::length(tangent) < 1e-6F) {
		return;
	}
	const Vec2 direction = Math::normalize(tangent);

	F32 start = std::numeric_limits<F32>::lowest();
	F32 stop = std::numeric_limits<F32>::max();
	const Vec2 low = f.viewport.position;
	const Vec2 high = f.viewport.position + f.viewport.size;
	for (int k = 0; k < 2; ++k) {
		if (Math::abs(direction[k]) < 1e-6F) {
			if (center[k] < low[k] || center[k] > high[k]) {
				return;
			}
			continue;
		}
		const F32 a = (low[k] - center[k]) / direction[k];
		const F32 b = (high[k] - center[k]) / direction[k];
		start = Math::max(start, Math::min(a, b));
		stop = Math::min(stop, Math::max(a, b));
	}

	if (Math::abs(toward.w) > 1e-6F) {
		const Vec2 vanishing = f.viewport.position + (((Vec2(toward) / toward.w) + Vec2(1.F)) * half_size);
		const F32 along = Math::dot(vanishing - center, direction);
		if (along > 0.F) {
			stop = Math::min(stop, along);
		} else {
			start = Math::max(start, along);
		}
	}

	if (start < stop) {
		draw_list.draw_line(center + (direction * start), center + (direction * stop), 1.5F,
							Vec4(Vec3(k_axis_colors[axis]), 0.8F), 1);
	}
}

void TransformGizmo::draw_translate(UI::Rendering::DrawList &draw_list) const {
	const bool active_axis_only = m_active && m_drag.handle != Handle::Center;
	for (Usize plane = 0; plane < 3 && !m_active; ++plane) {
		if (!m_frame.plane_visible[plane]) {
			continue;
		}
		const Vec4 color = color_for(plane_handle(plane), k_axis_colors[plane]);
		const Rect square = { .position = m_frame.plane_centers[plane] - Vec2(k_plane_half_px),
							  .size = Vec2(k_plane_half_px * 2.F) };
		draw_list.draw_rect(square, Vec4(Vec3(color), 0.35F), Vec4(2.F), 1.F, color, 1);
	}

	for (Usize axis = 0; axis < 3; ++axis) {
		const Handle handle = axis_handle(axis);
		const bool shown = !active_axis_only || handle == m_drag.handle ||
			(is_plane(m_drag.handle) && axis != normal_of(m_drag.handle));
		if (!m_frame.axis_visible[axis] || !shown) {
			continue;
		}
		const Vec4 color = color_for(handle, k_axis_colors[axis]);
		draw_list.draw_line(m_frame.center, m_frame.tips[axis], 2.5F, color, 1);
		draw_list.draw_rect({ .position = m_frame.tips[axis] - Vec2(5.F), .size = Vec2(10.F) }, color, Vec4(5.F), 0.F,
							Vec4(0.F), 2);
	}

	const Vec4 center = color_for(Handle::Center, k_center_color);
	draw_list.draw_rect({ .position = m_frame.center - Vec2(6.F), .size = Vec2(12.F) }, Vec4(0.F), Vec4(6.F), 1.5F,
						center, 3);
}

void TransformGizmo::draw_rotate(UI::Rendering::DrawList &draw_list) const {
	for (Usize axis = 0; axis < 3; ++axis) {
		const Handle handle = axis_handle(axis);
		if (m_active && handle != m_drag.handle) {
			continue;
		}
		const Vec4 color = color_for(handle, k_axis_colors[axis]);
		const auto &points = m_frame.rings[axis];
		for (Usize s = 0; s + 1 < points.size(); ++s) {
			draw_list.draw_line(points[s], points[s + 1], 2.5F, color, 2);
		}
	}

	if (!m_active) {
		return;
	}

	const Frame &f = m_drag.frame;
	if (is_axis(m_drag.handle) && !m_drag.modal) {
		const Usize axis = axis_of(m_drag.handle);
		const Vec3 u = f.axes[(axis + 1) % 3];
		const Vec3 v = f.axes[(axis + 2) % 3];
		const Vec4 fill = Vec4(Vec3(k_axis_colors[axis]), 0.9F);
		const int steps = Math::max(
			2, static_cast<int>(Math::abs(m_drag.applied_angle) / Math::TAU * static_cast<F32>(k_ring_segments)));
		Option<Vec2> previous;
		for (int s = 0; s <= steps; ++s) {
			const F32 theta =
				m_drag.ring_start_angle + (m_drag.applied_angle * static_cast<F32>(s) / static_cast<F32>(steps));
			const Vec3 point = f.origin + (((u * Math::cos(theta)) + (v * Math::sin(theta))) * (f.length * 0.8F));
			const Option<Vec2> projected = Rendering::project_to_screen(f.view, f.viewport, point);
			if (projected && previous) {
				draw_list.draw_line(*previous, *projected, 5.F, fill, 3);
			}
			if (projected && (s == 0 || s == steps)) {
				draw_list.draw_line(f.center, *projected, 1.5F, fill, 3);
			}
			previous = projected;
		}
	} else {
		draw_list.draw_line(f.center, m_drag.mouse, 1.5F, Vec4(Vec3(k_center_color), 0.7F), 3);
	}
}

void TransformGizmo::draw_scale(UI::Rendering::DrawList &draw_list) const {
	for (Usize axis = 0; axis < 3; ++axis) {
		const Handle handle = axis_handle(axis);
		if (!m_frame.axis_visible[axis] || (m_active && is_axis(m_drag.handle) && handle != m_drag.handle)) {
			continue;
		}
		const Vec4 color = color_for(handle, k_axis_colors[axis]);
		draw_list.draw_line(m_frame.center, m_frame.tips[axis], 2.5F, color, 1);
		draw_list.draw_rect({ .position = m_frame.tips[axis] - Vec2(5.F), .size = Vec2(10.F) }, color, Vec4(1.5F), 0.F,
							Vec4(0.F), 2);
	}

	const Vec4 center = color_for(Handle::Center, k_center_color);
	draw_list.draw_rect({ .position = m_frame.center - Vec2(6.F), .size = Vec2(12.F) }, Vec4(Vec3(center), 0.25F),
						Vec4(2.F), 1.5F, center, 3);
	if (m_active && m_drag.modal) {
		draw_list.draw_line(m_frame.center, m_drag.mouse, 1.5F, Vec4(Vec3(k_center_color), 0.7F), 3);
	}
}

void TransformGizmo::draw_readout(UI::Rendering::DrawList &draw_list) const {
	UI::Text::FontAtlas *atlas = font();
	if (atlas == nullptr) {
		return;
	}
	const std::string text = readout();
	const F32 text_size = font_size();
	atlas->ensure_glyphs(text);
	const Vec2 size = atlas->measure_text(text, text_size);
	const Vec2 padding(8.F, 4.F);
	Vec2 position = m_drag.mouse + Vec2(18.F, 18.F);
	const Rect viewport = m_frame.viewport;
	position.x = Math::min(position.x, viewport.right() - size.x - (padding.x * 2.F) - 4.F);
	position.y = Math::min(position.y, viewport.bottom() - size.y - (padding.y * 2.F) - 4.F);
	const Rect box = { .position = Math::round(position), .size = size + (padding * 2.F) };
	draw_list.draw_rect(box, Vec4(0.06F, 0.06F, 0.06F, 0.92F), Vec4(4.F), 1.F, Vec4(1.F, 1.F, 1.F, 0.08F), 4);
	draw_list.draw_text({ .position = box.position + padding, .size = size }, text, atlas,
						Vec4(0.93F, 0.93F, 0.93F, 1.F), text_size, UI::TextAlign::Left, 5);
}

} // namespace Editor
