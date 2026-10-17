#pragma once

#include "Aquila/Rendering/RenderView.h"
#include "Aquila/Scene/Entity.h"
#include "Aquila/Scene/SceneStatistics.h"
#include "Core/EditorWindow.h"

namespace Aquila::UI::Core {
class Button;
class DragFloat;
class Dropdown;
class Popup;
}

namespace Editor {

class TransformGizmo;
class ViewportOverlay;
class ViewportPanel;

class ViewportWindow final : public EditorWindow {
  public:
	explicit ViewportWindow(EditorContext &context);
	~ViewportWindow() override;

	void build(Aquila::UI::Core::View &content) override;
	void update(F32 delta_time) override;
	/// While hovered or while the gizmo drags, never while the camera is flying.
	[[nodiscard]] bool accepts_shortcuts(bool hovered) const override;

  private:
	void wire_tools(Aquila::UI::Core::View &content);
	void wire_shading(Aquila::UI::Core::View &content);
	void wire_show_menu(Aquila::UI::Core::View &content);
	void wire_camera_menu(Aquila::UI::Core::View &content);
	void refresh_camera_menu();
	void refresh_debug_info(F32 delta_time);
	[[nodiscard]] Aquila::Rendering::RenderView current_view() const;
	void refresh_tools();
	void request_pick(Vec2 uv);
	void register_commands();
	[[nodiscard]] bool can_constrain() const;

	Unique<ViewportPanel> m_panel;
	TransformGizmo *m_gizmo = nullptr;
	ViewportOverlay *m_overlay = nullptr;
	Aquila::UI::Core::Button *m_snap_button = nullptr;
	Aquila::UI::Core::Button *m_camera_button = nullptr;
	Aquila::UI::Core::Popup *m_camera_popup = nullptr;
	Aquila::UI::Core::DragFloat *m_fov_field = nullptr;
	Aquila::UI::Core::DragFloat *m_speed_field = nullptr;
	Aquila::UI::Core::Dropdown *m_view_through = nullptr;
	std::vector<Aquila::SceneManagement::Entity> m_scene_cameras;
	Aquila::SceneManagement::Entity m_view_camera;
	Aquila::SceneManagement::StatisticsCollector m_statistics;
	F32 m_debug_refresh_timer = 0.F;
	std::array<Aquila::UI::Core::Button *, 3> m_tool_buttons{};
	Aquila::UI::Core::Button *m_space_button = nullptr;
};

}
