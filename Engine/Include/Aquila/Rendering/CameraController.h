#ifndef AQUILA_RENDERING_CAMERA_CONTROLLER_H
#define AQUILA_RENDERING_CAMERA_CONTROLLER_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Rendering/Camera.h"
#include "Aquila/Rendering/RenderView.h"

namespace Aquila::Platform::Events {
class Event;
}

namespace Aquila::Rendering {

/// The editor camera. Hold RMB to fly (WASDQE, mouse to look), Alt+LMB to orbit, scroll to dolly.
/// It requests frames itself while it moves.
class CameraController {
  public:
	CameraController();

	void set_viewport_size(Uint32 width, Uint32 height);
	/// Navigation only starts when the mouse is inside this rect.
	void set_viewport_rect(Vec2 position, Vec2 size);
	void set_navigation_blocked(bool blocked) { m_navigation_blocked = blocked; }

	void update(F32 delta_time);
	void on_event(Platform::Events::Event &event);

	[[nodiscard]] RenderView get_render_view() const;
	/// The view of a scene camera with this world matrix (+Z forward, +Y up). Same conventions as the editor camera.
	[[nodiscard]] RenderView render_view_from(const Mat4 &world, F32 fov_degrees, F32 near_plane, F32 far_plane) const;

	void set_fov(F32 fov_degrees);
	[[nodiscard]] F32 get_fov() const { return m_fov; }
	void set_move_speed(F32 speed) { m_move_speed = speed; }
	[[nodiscard]] F32 get_move_speed() const { return m_move_speed; }
	void set_ev100(F32 ev100);
	[[nodiscard]] F32 get_ev100() const { return m_ev100; }
	void set_auto_exposure(bool enabled);
	[[nodiscard]] bool get_auto_exposure() const { return m_auto_exposure; }
	/// Stops, + is brighter. Only used with auto exposure.
	void set_exposure_compensation(F32 stops);
	[[nodiscard]] F32 get_exposure_compensation() const { return m_exposure_compensation; }

  private:
	enum class NavMode : Uint8 { None, Fly, Orbit };

	Camera m_camera;

	F32 m_fov = 60.F;
	F32 m_near = 0.1F;
	F32 m_far = 1000.F;
	F32 m_aspect = 16.F / 9.F;
	F32 m_move_speed = 5.F;
	F32 m_ev100 = Foundation::Photometry::DAYLIGHT_EV100;
	F32 m_exposure_compensation = 0.F;
	bool m_auto_exposure = true;

	Vec2 m_viewport_pos{ 0.F, 0.F };
	Vec2 m_viewport_size{ 0.F, 0.F };
	Uint32 m_width = 0;
	Uint32 m_height = 0;

	NavMode m_nav_mode = NavMode::None;
	Vec2 m_last_mouse{ 0.F, 0.F };
	bool m_prev_rmb = false;
	bool m_prev_lmb = false;
	bool m_navigation_blocked = false;
};

} // namespace Aquila::Rendering

#endif
