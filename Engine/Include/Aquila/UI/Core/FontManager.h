#ifndef AQUILA_UI_CORE_FONT_MANAGER_H
#define AQUILA_UI_CORE_FONT_MANAGER_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/Registration.h"
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

	/// Family names from every font folder, sorted, each once. A family is a subfolder with a .ttf/.otf in it.
	[[nodiscard]] std::vector<std::string> available_families() const;
	/// The file a family loads: the one with "Regular" in its name, else the first .ttf/.otf by name. When two folders
	/// have the family, the folder added last wins. nullopt when no folder has it.
	[[nodiscard]] Option<std::string> family_file(const std::string &family) const;
	/// Adds a VFS folder of font families next to /resources/Engine/Fonts. Call reload() to use a family from it.
	[[nodiscard]] Foundation::Registration add_font_dir(std::string directory);

	/// Copies a .ttf/.otf from anywhere on disk into `directory`/<family>/, the family name read from the file (the
	/// file name before its first '-' when the font has none). Overwrites a file of the same name. Main thread only.
	/// @return the family, nullopt when the file can't be read, isn't a font or can't be written (logged)
	static Option<std::string> import_font(const std::string &native_path, const std::string &directory);

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

	struct FontDir {
		Uint64 id = 0;
		std::string path;
	};

	bool m_initialized = false;
	std::vector<Unique<Text::FontAtlas>> m_atlases;
	std::unordered_map<std::string, Text::FontAtlas *> m_font_map;
	std::vector<FontDir> m_font_dirs;
	Uint64 m_next_dir_id = 1;
	Foundation::RegistrationOwner m_registrations;
};

} // namespace Aquila::UI::Core

#endif
