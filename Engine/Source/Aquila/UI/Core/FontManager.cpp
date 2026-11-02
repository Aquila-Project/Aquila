#include "Aquila/UI/Core/FontManager.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/UI/Core/FontRegistry.h"
#include "Aquila/UI/Text/FontFace.h"

namespace Aquila::UI::Core {

namespace {

constexpr std::string_view K_FONTS_DIR = "/resources/Engine/Fonts";
constexpr std::string_view K_FALLBACK_FONT = "/resources/Engine/Fonts/Outfit/Outfit-Regular.ttf";
constexpr std::string_view K_UNSAFE_NAME_CHARS = "/\\:*?\"<>|";

bool is_font_file(std::string_view file) {
	const std::string lower = Foundation::to_lower(file);
	return lower.ends_with(".ttf") || lower.ends_with(".otf");
}

std::vector<std::string> font_files(const std::string &dir) {
	std::vector<std::string> files = Platform::Filesystem::VirtualFileSystem::get()->list_directory(dir);
	std::erase_if(files, [](const std::string &file) { return !is_font_file(file); });
	std::ranges::sort(files);
	return files;
}

bool is_family_name(const std::string &family) {
	return !family.empty() && family != "." && family != ".." &&
		family.find_first_of(K_UNSAFE_NAME_CHARS) == std::string::npos;
}

std::string folder_name(std::string_view name) {
	std::string out;
	for (const char c : name) {
		if (K_UNSAFE_NAME_CHARS.find(c) == std::string_view::npos && static_cast<Uint8>(c) >= 0x20) {
			out += c;
		}
	}
	const std::string trimmed(Foundation::trim(out));
	return trimmed.find_first_not_of('.') == std::string::npos ? std::string{} : trimmed;
}
}

std::vector<std::string> FontManager::available_families() const {
	std::vector<std::string> families;
	auto *vfs = Platform::Filesystem::VirtualFileSystem::get();
	if (vfs == nullptr) {
		return families;
	}
	std::vector<std::string> dirs{ std::string(K_FONTS_DIR) };
	for (const FontDir &dir : m_font_dirs) {
		dirs.push_back(dir.path);
	}
	for (const std::string &dir : dirs) {
		for (const std::string &entry : vfs->list_directory(dir)) {
			if (vfs->is_directory(dir + "/" + entry) && !font_files(dir + "/" + entry).empty()) {
				families.push_back(entry);
			}
		}
	}
	std::ranges::sort(families);
	const auto [first, last] = std::ranges::unique(families);
	families.erase(first, last);
	return families;
}

Option<std::string> FontManager::family_file(const std::string &family) const {
	auto *vfs = Platform::Filesystem::VirtualFileSystem::get();
	if (vfs == nullptr || !is_family_name(family)) {
		return std::nullopt;
	}
	std::vector<std::string> dirs{ std::string(K_FONTS_DIR) };
	for (const FontDir &dir : m_font_dirs) {
		dirs.push_back(dir.path);
	}
	for (auto dir = dirs.rbegin(); dir != dirs.rend(); ++dir) {
		const std::string family_dir = *dir + "/" + family;
		if (!vfs->is_directory(family_dir)) {
			continue;
		}
		const std::vector<std::string> files = font_files(family_dir);
		if (files.empty()) {
			continue;
		}
		const auto regular = std::ranges::find_if(
			files, [](const std::string &file) { return file.find("Regular") != std::string::npos; });
		return family_dir + "/" + (regular != files.end() ? *regular : files.front());
	}
	return std::nullopt;
}

Foundation::Registration FontManager::add_font_dir(std::string directory) {
	const Uint64 id = m_next_dir_id++;
	m_font_dirs.push_back({ .id = id, .path = std::move(directory) });
	return m_registrations.make(
		[this, id] { std::erase_if(m_font_dirs, [id](const FontDir &dir) { return dir.id == id; }); });
}

Option<std::string> FontManager::import_font(const std::string &native_path, const std::string &directory) {
	std::ifstream in(native_path, std::ios::binary);
	if (!in) {
		AQUILA_LOG_ERROR("FontManager: can't read '{}'", native_path);
		return std::nullopt;
	}
	const std::vector<Uint8> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	const Option<std::string> name = Text::FontFace::read_family_name(data.data(), data.size());
	if (!name) {
		AQUILA_LOG_ERROR("FontManager: '{}' isn't a TrueType or OpenType font", native_path);
		return std::nullopt;
	}

	const std::string file_name = std::filesystem::path(native_path).filename().string();
	std::string family = folder_name(*name);
	if (family.empty()) {
		family = folder_name(file_name.substr(0, file_name.find_first_of("-.")));
	}
	if (family.empty()) {
		AQUILA_LOG_ERROR("FontManager: '{}' has no family name to file it under", native_path);
		return std::nullopt;
	}

	auto *vfs = Platform::Filesystem::VirtualFileSystem::get();
	const std::string family_dir = directory + "/" + family;
	if (vfs == nullptr || (!vfs->is_directory(directory) && !vfs->create_dir(directory)) ||
		(!vfs->is_directory(family_dir) && !vfs->create_dir(family_dir))) {
		AQUILA_LOG_ERROR("FontManager: can't create '{}'", family_dir);
		return std::nullopt;
	}
	auto out = vfs->open_file(family_dir + "/" + file_name, AccessMode::Write, OpenMode::Binary);
	if (!out || !out->is_valid() || out->write(data.data(), data.size()) != data.size()) {
		AQUILA_LOG_ERROR("FontManager: can't write '{}/{}'", family_dir, file_name);
		return std::nullopt;
	}
	AQUILA_LOG_INFO("FontManager: added font family '{}' from '{}'", family, native_path);
	return family;
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
		if (!atlas && path != K_FALLBACK_FONT) {
			AQUILA_LOG_ERROR("FontManager: failed to load '{}' from {}, using Outfit", name, path);
			atlas = Text::FontAtlas::create_from_file(ctx, std::string(K_FALLBACK_FONT));
		}
		if (!atlas) {
			AQUILA_LOG_ERROR("FontManager: failed to load '{}' from {}", name, path);
			return;
		}
		FontRegistry::Register(name, atlas.get());
		font_map[name] = atlas.get();
		m_atlases.push_back(std::move(atlas));
	};

	const auto path_of = [this](const std::string &family) {
		if (Option<std::string> file = family_file(family)) {
			return *file;
		}
		AQUILA_LOG_WARNING("FontManager: no font family '{}', using Outfit", family);
		return std::string(K_FALLBACK_FONT);
	};
	load("regular", path_of(settings.main_family));
	load("mono", path_of(settings.mono_family));

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
