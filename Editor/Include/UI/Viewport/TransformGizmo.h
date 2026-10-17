#pragma once

#include "Aquila/Rendering/RenderView.h"
#include "Aquila/Scene/Entity.h"
#include "Core/EditorTools.h"
#include "UI/Viewport/ViewportCanvas.h"

namespace Editor {

class EditorContext;

/// Move, rotate and scale handles drawn in 2D from projected 3D points. A drag freezes its frame
/// (origin, axes, view) when it starts, so the handles don't jitter while the object moves.
class TransformGizmo final : public ViewportCanvas {
  public:
	explicit TransformGizmo(EditorContext &context);

	[[nodiscard]] std::string_view get_type_name() const override { return "TransformGizmo"; }

	/// Call it every frame, it only redraws when something changed.
	void sync(const Rect &viewport, const Aquila::Rendering::RenderView &view);
	[[nodiscard]] bool is_over_handle() const { return m_hovered != Handle::None || m_active; }
	[[nodiscard]] bool is_active() const { return m_active; }
	[[nodiscard]] bool is_modal() const { return m_active && m_drag.modal; }

	/// Blender style modal transform on axis 0, 1 or 2. Again on the same axis switches to the local axis,
	/// a third time frees it.
	void constrain_to_axis(int axis);
	void confirm();
	void cancel();

	void on_mouse_press(Aquila::Platform::MouseButton btn, Vec2 pos) override;
	void on_mouse_move(Vec2 pos) override;
	void on_mouse_hover(Vec2 pos) override;
	void on_mouse_release(Aquila::Platform::MouseButton btn, Vec2 pos) override;
	void draw(Aquila::UI::Rendering::DrawList &draw_list) const override;

	/// Presses that miss every handle, the viewport picks with them.
	Signal<void(Vec2)> on_background_pressed;

  private:
	enum class Handle : Uint8 { None, X, Y, Z, PlaneYZ, PlaneXZ, PlaneXY, Center };

	struct Frame {
		bool valid = false;
		TransformTool tool = TransformTool::Translate;
		Aquila::Rendering::RenderView view;
		Rect viewport;
		Vec3 origin{};
		std::array<Vec3, 3> axes{};
		F32 length = 1.F;
		Vec2 center{};
		std::array<Vec2, 3> tips{};
		std::array<bool, 3> axis_visible{};
		std::array<Vec2, 3> plane_centers{};
		std::array<bool, 3> plane_visible{};
		std::array<std::vector<Vec2>, 3> rings;
		std::array<std::vector<bool>, 3> ring_front;

		bool operator==(const Frame &) const = default;
	};

	struct Drag {
		Handle handle = Handle::None;
		TransformTool tool = TransformTool::Translate;
		bool modal = false;
		bool local_axes = false;
		Aquila::SceneManagement::Entity entity;
		Frame frame;
		Vec2 start_mouse{};
		Vec2 mouse{};
		Vec3 start_position{};
		Quaternion start_rotation{ 1.F, 0.F, 0.F, 0.F };
		Vec3 start_scale{ 1.F };
		Mat4 parent_world{ 1.F };
		Vec3 start_hit{};
		F32 start_param = 0.F;
		Vec2 ring_tangent{};
		F32 ring_start_angle = 0.F;
		F32 previous_angle = 0.F;
		F32 total_angle = 0.F;
		F32 applied_angle = 0.F;
		Vec3 applied_offset{};
		Vec3 applied_scale{ 1.F };
	};

	[[nodiscard]] static Handle axis_handle(Usize axis);
	[[nodiscard]] static Handle plane_handle(Usize normal);
	[[nodiscard]] static bool is_axis(Handle handle);
	[[nodiscard]] static bool is_plane(Handle handle);
	[[nodiscard]] static Usize axis_of(Handle handle);
	[[nodiscard]] static Usize normal_of(Handle handle);

	[[nodiscard]] Frame compute_frame(const Rect &viewport, const Aquila::Rendering::RenderView &view,
									  bool local_axes) const;
	[[nodiscard]] bool wants_local_axes(TransformTool tool) const;
	[[nodiscard]] Handle pick(Vec2 point) const;
	[[nodiscard]] Handle hovered_at(Vec2 point) const;
	[[nodiscard]] bool snapping() const;
	[[nodiscard]] Vec4 color_for(Handle handle, Vec4 base) const;
	[[nodiscard]] std::string readout() const;

	bool begin(Handle handle, Vec2 mouse, bool modal, bool local_axes);
	void update_drag(Vec2 mouse);
	void apply_translate(Vec2 mouse);
	void apply_rotate(Vec2 mouse);
	void apply_scale(Vec2 mouse);
	void restore_start();
	void finish();
	void publish_change();

	void draw_translate(Aquila::UI::Rendering::DrawList &draw_list) const;
	void draw_rotate(Aquila::UI::Rendering::DrawList &draw_list) const;
	void draw_scale(Aquila::UI::Rendering::DrawList &draw_list) const;
	void draw_constraint(Aquila::UI::Rendering::DrawList &draw_list) const;
	void draw_readout(Aquila::UI::Rendering::DrawList &draw_list) const;

	EditorContext &m_context;
	Frame m_frame;
	Handle m_hovered = Handle::None;
	bool m_active = false;
	bool m_left_was_down = false;
	bool m_right_was_down = false;
	bool m_drawn_snapping = false;
	Drag m_drag;
};

}
