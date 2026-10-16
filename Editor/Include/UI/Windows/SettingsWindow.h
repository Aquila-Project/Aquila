#pragma once

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Core/EditorConfig.h"

namespace Aquila::Graphics {
class Renderer2D;
}
namespace Aquila::GFX {
class GfxCommandList;
class GfxTexture;
}
namespace Aquila::Platform::Events {
class Event;
}
namespace Aquila::UI::Core {
class Canvas;
class View;
class TextureCache;
} // namespace Aquila::UI::Core

namespace Editor {

class SettingsWindow {
  public:
	SettingsWindow();
	~SettingsWindow();

	void build(Aquila::UI::Core::TextureCache *texture_cache, Uint32 width, Uint32 height,
			   const std::vector<std::string> &style_paths);

	void update(F32 delta_time);
	void render(Aquila::Graphics::Renderer2D &batcher, Aquila::GFX::GfxCommandList &cmd);
	void on_event(Aquila::Platform::Events::Event &event);
	void set_frame_target(const void *target);

	Delegate<void()> on_request_close;
	Delegate<void()> on_applied;

  private:
	Aquila::UI::Core::View *add_section(Aquila::UI::Core::View *host, const std::string &title);
	Aquila::UI::Core::View *make_row(Aquila::UI::Core::View *host, const std::string &label,
									 const std::string &description);
	void float_row(Aquila::UI::Core::View *host, const std::string &label, const std::string &description, float *field,
				   float min, float max, float speed, int precision);
	void font_row(Aquila::UI::Core::View *host, const std::string &label, const std::string &description,
				  std::string *family_field);

	void apply();
	void reset();
	void sync_widgets();

	Unique<Aquila::UI::Core::Canvas> m_canvas;
	Aquila::UI::Core::TextureCache *m_texture_cache = nullptr;
	Aquila::GFX::GfxTexture *m_help_icon = nullptr;
	Config::EditorPreferences m_working;
	std::vector<Delegate<void()>> m_sync;
};

} // namespace Editor
