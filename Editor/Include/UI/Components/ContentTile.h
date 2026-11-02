#ifndef AQUILA_EDITOR_CONTENT_TILE_H
#define AQUILA_EDITOR_CONTENT_TILE_H

#include "Aquila/UI/Core/Behaviour.h"
#include "Aquila/UI/Widgets/AssetPayload.h"
#include "Core/EditorConfig.h"

namespace Aquila::GFX {
class GfxTexture;
}

namespace Aquila::UI::Core {
class Clickable;
class Image;
class Label;
}

namespace Editor {

class ContentTile final : public Aquila::UI::Core::Behaviour {
  public:
	void define_behaviour() override;
	void show(Aquila::UI::Core::AssetPayload asset, bool is_directory, Aquila::GFX::GfxTexture *icon, bool thumbnail,
			  const Config::ContentBrowserPreferences &prefs);
	void set_thumbnail(Aquila::GFX::GfxTexture *texture);
	void start_rename(Delegate<void(Option<std::string>)> done);
	[[nodiscard]] Aquila::UI::Core::Clickable *get_clickable() const { return m_clickable; }

  private:
	Aquila::UI::Core::AssetPayload m_payload;
	F32 m_size = 48.F;
	Aquila::UI::Core::Clickable *m_clickable = nullptr;
	Aquila::UI::Core::View *m_preview = nullptr;
	Aquila::UI::Core::Image *m_image = nullptr;
	Aquila::UI::Core::View *m_badge = nullptr;
	Aquila::UI::Core::Label *m_badge_text = nullptr;
	Aquila::UI::Core::Label *m_name = nullptr;
};

}

#endif
