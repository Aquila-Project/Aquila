#include "Aquila/Rendering/CameraController.h"

#include "Aquila/Platform/Events/InputEvent.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Platform/Input.h"
#include "Aquila/Foundation/FrameScheduler.h"

namespace Aquila::Rendering {

using Platform::Input;
namespace Events = Platform::Events;

namespace {
constexpr F32 kRotateSensitivity = 0.0035F;
constexpr F32 kOrbitSensitivity = 0.0075F;
constexpr F32 kDollyStep = 0.15F;

bool point_in_rect(Vec2 point, Vec2 position, Vec2 size) {
	return point.x >= position.x && point.y >= position.y && point.x < position.x + size.x &&
		point.y < position.y + size.y;
}
} // namespace

CameraController::CameraController() {
	m_camera.set_position({ 0.F, 1.5F, -5.F });
	m_camera.get_rotation() = Vec3(0.F);
	m_camera.set_perspective_projection(m_fov, m_aspect, m_near, m_far);
	m_camera.set_view_yxz(m_camera.get_position(), m_camera.get_rotation());
}

void CameraController::set_viewport_size(Uint32 width, Uint32 height) {
	if (width == 0 || height == 0 || (width == m_width && height == m_height)) {
		return;
	}
	m_width = width;
	m_height = height;
	m_aspect = static_cast<F32>(width) / static_cast<F32>(height);
	m_camera.set_perspective_projection(m_fov, m_aspect, m_near, m_far);
}

void CameraController::set_fov(F32 fov_degrees) {
	const F32 fov = Math::clamp(fov_degrees, 1.F, 179.F);
	if (fov == m_fov) {
		return;
	}
	m_fov = fov;
	m_camera.set_perspective_projection(m_fov, m_aspect, m_near, m_far);
	Foundation::FrameScheduler::get()->request_frame();
}

void CameraController::set_ev100(F32 ev100) {
	if (ev100 == m_ev100) {
		return;
	}
	m_ev100 = ev100;
	Foundation::FrameScheduler::get()->request_frame();
}

void CameraController::set_auto_exposure(bool enabled) {
	if (enabled == m_auto_exposure) {
		return;
	}
	m_auto_exposure = enabled;
	Foundation::FrameScheduler::get()->request_frame();
}

void CameraController::set_exposure_compensation(F32 stops) {
	if (stops == m_exposure_compensation) {
		return;
	}
	m_exposure_compensation = stops;
	Foundation::FrameScheduler::get()->request_frame();
}

RenderView CameraController::render_view_from(const Mat4 &world, F32 fov_degrees, F32 near_plane,
											  F32 far_plane) const {
	Mat4 basis(1.F);
	for (int i = 0; i < 3; ++i) {
		const Vec3 axis = Vec3(world[i]);
		const F32 length = Math::length(axis);
		basis[i] = Vec4(length > Math::EPSILON ? axis / length : Vec3(0.F), 0.F);
	}
	basis[3] = world[3];

	RenderView view;
	view.view = Math::inverse(basis);
	view.projection = Math::perspective_vulkan(Math::radians(fov_degrees), m_aspect, near_plane, far_plane);
	view.projection[1][1] *= -1.F;
	view.position = Vec3(basis[3]);
	view.right = Vec3(basis[0]);
	view.up = Vec3(basis[1]);
	view.forward = Vec3(basis[2]);
	view.near_plane = near_plane;
	view.far_plane = far_plane;
	view.fov = fov_degrees;
	view.aspect = m_aspect;
	view.is_orthographic = false;
	view.valid = true;
	return view;
}

void CameraController::set_viewport_rect(Vec2 position, Vec2 size) {
	m_viewport_pos = position;
	m_viewport_size = size;
}

void CameraController::update(F32 delta_time) {
	const Vec2 mouse = Input::get_mouse_position();
	const bool rmb = Input::is_mouse_button_pressed(Events::MouseButton::Right);
	const bool lmb = Input::is_mouse_button_pressed(Events::MouseButton::Left);
	const bool alt = Input::is_key_pressed(Events::KeyCode::LeftAlt);
	const bool over = !m_navigation_blocked && point_in_rect(mouse, m_viewport_pos, m_viewport_size);

	const bool rmb_edge = rmb && !m_prev_rmb;
	const bool lmb_edge = lmb && !m_prev_lmb;
	m_prev_rmb = rmb;
	m_prev_lmb = lmb;

	if (m_nav_mode == NavMode::None) {
		if (rmb_edge && over) {
			m_nav_mode = NavMode::Fly;
			m_last_mouse = mouse;
		} else if (lmb_edge && over && alt) {
			m_nav_mode = NavMode::Orbit;
			m_last_mouse = mouse;
		}
	}

	if (m_nav_mode == NavMode::Fly && !rmb) {
		m_nav_mode = NavMode::None;
	}
	if (m_nav_mode == NavMode::Orbit && !lmb) {
		m_nav_mode = NavMode::None;
	}

	if (m_nav_mode == NavMode::None) {
		return;
	}

	const Vec2 delta = mouse - m_last_mouse;
	m_last_mouse = mouse;

	if (m_nav_mode == NavMode::Orbit) {
		m_camera.orbit_rotate(delta.x * kOrbitSensitivity, -delta.y * kOrbitSensitivity);
		return;
	}

	m_camera.rotate(delta.x * kRotateSensitivity, delta.y * kRotateSensitivity);

	m_camera.reset_speed();
	m_camera.get_movement_speed() = m_move_speed;
	if (Input::is_key_pressed(Events::KeyCode::LeftShift)) {
		m_camera.speed_up();
	}

	bool moving = false;
	if (Input::is_key_pressed(Events::KeyCode::W)) {
		m_camera.move_forward(delta_time);
		moving = true;
	}
	if (Input::is_key_pressed(Events::KeyCode::S)) {
		m_camera.move_backward(delta_time);
		moving = true;
	}
	if (Input::is_key_pressed(Events::KeyCode::D)) {
		m_camera.move_right(delta_time);
		moving = true;
	}
	if (Input::is_key_pressed(Events::KeyCode::A)) {
		m_camera.move_left(delta_time);
		moving = true;
	}
	if (Input::is_key_pressed(Events::KeyCode::E)) {
		m_camera.get_position().y += m_camera.get_movement_speed() * delta_time;
		moving = true;
	}
	if (Input::is_key_pressed(Events::KeyCode::Q)) {
		m_camera.get_position().y -= m_camera.get_movement_speed() * delta_time;
		moving = true;
	}

	m_camera.set_view_yxz(m_camera.get_position(), m_camera.get_rotation());

	if (moving) {
		Foundation::FrameScheduler::get()->request_frame();
	}
}

void CameraController::on_event(Events::Event &event) {
	Events::EventDispatcher dispatcher(event);
	dispatcher.dispatch<Events::MouseMovedEvent>([this](Events::MouseMovedEvent &) {
		if (m_nav_mode != NavMode::None) {
			Foundation::FrameScheduler::get()->request_frame();
		}
		return false;
	});
	dispatcher.dispatch<Events::MouseScrolledEvent>([this](Events::MouseScrolledEvent &scroll) {
		if (m_navigation_blocked || !point_in_rect(Input::get_mouse_position(), m_viewport_pos, m_viewport_size)) {
			return false;
		}
		m_camera.move_forward(scroll.get_y_offset() * kDollyStep);
		m_camera.set_view_yxz(m_camera.get_position(), m_camera.get_rotation());
		Foundation::FrameScheduler::get()->request_frame();
		return true;
	});
}

RenderView CameraController::get_render_view() const {
	const Mat4 &inverse_view = m_camera.get_inverse_view();

	RenderView view;
	view.view = m_camera.get_view();
	view.projection = m_camera.get_projection();
	view.projection[1][1] *= -1.F;
	view.position = Vec3(inverse_view[3]);
	view.right = Aquila::Math::normalize(Vec3(inverse_view[0]));
	view.up = Aquila::Math::normalize(Vec3(inverse_view[1]));
	view.forward = Aquila::Math::normalize(Vec3(inverse_view[2]));
	view.near_plane = m_near;
	view.far_plane = m_far;
	view.fov = m_fov;
	view.aspect = m_aspect;
	view.ev100 = m_ev100;
	view.exposure_compensation = m_exposure_compensation;
	view.auto_exposure = m_auto_exposure;
	view.is_orthographic = false;
	view.valid = true;
	return view;
}

} // namespace Aquila::Rendering
