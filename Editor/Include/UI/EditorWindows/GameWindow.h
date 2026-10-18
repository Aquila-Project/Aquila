#pragma once

#include "Aquila/Rendering/RenderSettings.h"
#include "Core/EditorWindow.h"

namespace Aquila::UI::Core {
class Button;
class Image;
class Label;
}

namespace Editor {

/// Godot style Game view: the running game through the scene's active camera, drawn with the settings a built
/// game uses (RenderSettings::game()). Play switches to the Game workspace that holds it.
///
/// While live it tells EditorContext where the game is (get_game_view()), EditorModule gives the game input focus
/// on a click inside and feeds it the main window's events from then on.
///
/// @note There's one 3D render per frame. While the game plays and this window is on screen it owns that render,
/// so a viewport showing at the same time hides its gizmo and overlays and shows the game too.
class GameWindow final : public EditorWindow {
  public:
	static constexpr std::string_view k_type_id = "game";

	explicit GameWindow(EditorContext &context);
	~GameWindow() override;

	void build(Aquila::UI::Core::View &content) override;
	void update(F32 delta_time) override;

	/// The game plays (or is paused) and a Game window is on screen.
	[[nodiscard]] static bool is_live(const EditorContext &context);

  private:
	void set_live(bool live);

	Aquila::UI::Core::Image *m_image = nullptr;
	Aquila::UI::Core::View *m_stage = nullptr;
	Aquila::UI::Core::View *m_idle = nullptr;
	Aquila::UI::Core::View *m_no_camera = nullptr;
	Aquila::UI::Core::Label *m_camera_name = nullptr;
	Aquila::UI::Core::Button *m_pause_button = nullptr;
	/// The editor's settings while the game view owns the render, put back when it stops.
	Option<Aquila::Rendering::RenderSettings> m_editor_settings;
};

}
