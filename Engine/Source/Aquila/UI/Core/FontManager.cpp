#include "Aquila/UI/Core/FontManager.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/UI/Core/FontRegistry.h"

namespace Aquila::UI::Core {

namespace {

constexpr std::string_view k_fonts_dir = "/resources/Engine/Fonts";
constexpr std::string_view k_fallback_font = "/resources/Engine/Fonts/Outfit/Outfit-Regular.ttf";

std::string family_to_path(const std::string &family) {
	auto *vfs = Platform::Filesystem::VirtualFileSystem::get();
	const std::string dir = std::string(k_fonts_dir) + "/" + family;
	if (family.empty() || family.find('/') != std::string::npos || vfs == nullptr || !vfs->is_directory(dir)) {
		AQUILA_LOG_WARNING("FontManager: no font family '{}', using Outfit", family);
		return std::string(k_fallback_font);
	}
	std::vector<std::string> files = vfs->list_directory(dir);
	std::erase_if(files, [](const std::string &file) { return !file.ends_with(".ttf") && !file.ends_with(".otf"); });
	std::ranges::sort(files);
	const auto regular = std::ranges::find_if(files, [](const std::string &file) {
		return file.find("Regular") != std::string::npos;
	});
	if (regular != files.end()) {
		return dir + "/" + *regular;
	}
	return files.empty() ? std::string(k_fallback_font) : dir + "/" + files.front();
}

}

std::vector<std::string> FontManager::available_families() {
	std::vector<std::string> families;
	auto *vfs = Platform::Filesystem::VirtualFileSystem::get();
	if (vfs == nullptr) {
		return families;
	}
	for (const std::string &entry : vfs->list_directory(std::string(k_fonts_dir))) {
		if (vfs->is_directory(std::string(k_fonts_dir) + "/" + entry)) {
			families.push_back(entry);
		}
	}
	std::ranges::sort(families);
	return families;
}

FontManager &FontManager::get() {
	static FontManager instance;
	return instance;
}

void FontManager::initialize(GFX::GfxContext &ctx, const FontSettings &settings) {
	if (m_initialized) {
		AQUILA_LOG_WARNING("FontManager: already initialized");
		return;
	}
	reload(ctx, settings);
}

void FontManager::reload(GFX::GfxContext &ctx, const FontSettings &settings) {
	std::unordered_map<std::string, Text::FontAtlas *> font_map;

	auto load = [&](const char *name, const std::string &path) {
		auto atlas = Text::FontAtlas::create_from_file(ctx, path);
		if (!atlas) {
			AQUILA_LOG_ERROR("FontManager: failed to load '{}' from {}", name, path);
			return;
		}
		FontRegistry::Register(name, atlas.get());
		font_map[name] = atlas.get();
		m_atlases.push_back(std::move(atlas));
	};

	load("regular", family_to_path(settings.main_family));
	load("mono", family_to_path(settings.mono_family));

	if (auto it = font_map.find("regular"); it != font_map.end()) {
		FontRegistry::Register("medium", it->second);
		FontRegistry::Register("bold", it->second);
		font_map["medium"] = it->second;
		font_map["bold"] = it->second;
	}

	m_font_map = std::move(font_map);
	m_initialized = true;
	AQUILA_LOG_INFO("FontManager: main '{}', mono '{}' at {}pt, {} atlases retained", settings.main_family,
					settings.mono_family, settings.size, m_atlases.size());
}

void FontManager::shutdown() {
	m_font_map.clear();
	m_atlases.clear();
	m_initialized = false;
}

Text::FontAtlas *FontManager::get_font(const std::string &name) const {
	auto it = m_font_map.find(name);
	if (it != m_font_map.end()) {
		return it->second;
	}
	AQUILA_LOG_WARNING("FontManager: font '{}' not found, returning fallback", name);
	return m_atlases.empty() ? nullptr : m_atlases.front().get();
}

} // namespace Aquila::UI::Core
