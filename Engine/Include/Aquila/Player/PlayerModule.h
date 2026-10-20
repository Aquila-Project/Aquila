#ifndef AQUILA_PLAYER_PLAYER_MODULE_H
#define AQUILA_PLAYER_PLAYER_MODULE_H

#include "Aquila/Application/IModule.h"
#include "Aquila/Player/BuildLayout.h"
#include "Aquila/Player/BuildManifest.h"

namespace Aquila::Player {

/// Runs a built game: mounts its project, opens the startup scene through the primary camera and starts playing.
/// Every event of the window goes to the game input.
/// Any failure in on_attach() is logged and closes the game.
class PlayerModule final : public Application::IModule {
  public:
	PlayerModule(BuildLayout layout, BuildManifest manifest);

	void on_attach(Application::EngineContext &engine) override;
	void on_event(Platform::Events::Event &event) override;
	void on_render_resize(Uint32 width, Uint32 height) override;

  private:
	void fit_camera(Uint32 width, Uint32 height) const;

	BuildLayout m_layout;
	BuildManifest m_manifest;
	Application::EngineContext *m_engine = nullptr;
};

} // namespace Aquila::Player

#endif
