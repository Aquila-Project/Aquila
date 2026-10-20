#ifndef AQUILA_UI_CORE_FONT_MANAGER_H
#define AQUILA_UI_CORE_FONT_MANAGER_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/UI/Text/FontAtlas.h"

namespace Aquila::GFX {
class GfxContext;
}

namespace Aquila::UI::Core {

struct FontSettings {
	std::string main_family = "Geist";
	std::string mono_family = "Inconsolata";
	F32 size = 16.F;

	bool operator==(const FontSettings &) const = default;
};

class FontManager {
  public:
	static FontManager &get();

	/// Folder names under /resources/Engine/Fonts, sorted. A family is any folder there with a .ttf/.otf in it, the
	/// one with "Regular" in its name wins.
	[[nodiscard]] static std::vector<std::string> available_families();

	void initialize(GFX::GfxContext &ctx, const FontSettings &settings);
	void reload(GFX::GfxContext &ctx, const FontSettings &settings);
	void shutdown();

	[[nodiscard]] bool is_initialized() const { return m_initialized; }
	/// "regular", "medium", "bold" or "mono". Null for anything else.
	[[nodiscard]] Text::FontAtlas *get_font(const std::string &name) const;

  private:
	FontManager() = default;
	~FontManager() = default;
	FontManager(const FontManager &) = delete;
	FontManager &operator=(const FontManager &) = delete;

	bool m_initialized = false;
	std::vector<Unique<Text::FontAtlas>> m_atlases;
	std::unordered_map<std::string, Text::FontAtlas *> m_font_map;
};

} // namespace Aquila::UI::Core

#endif
