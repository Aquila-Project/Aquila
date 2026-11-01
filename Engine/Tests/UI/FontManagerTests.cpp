#include <doctest.h>

#include "Aquila/Platform/Filesystem/NativeFileSystem.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/UI/Core/FontManager.h"
#include "Aquila/UI/Text/FontFace.h"

#include "VfsTestSupport.h"

using namespace Aquila;
using namespace Aquila::UI;
using namespace Aquila::UI::Core;
using Aquila::Platform::Filesystem::VirtualFileSystem;

namespace {

constexpr std::string_view K_MOUNT = "/font_tests";

std::vector<Uint8> read_bytes(const std::string &virtual_path) {
	auto file = VirtualFileSystem::get()->open_file(virtual_path, AccessMode::Read, OpenMode::Binary);
	REQUIRE(file != nullptr);
	std::vector<Uint8> data(static_cast<Usize>(file->size()));
	file->read(data.data(), data.size());
	return data;
}

std::string native(const std::string &virtual_path) {
	const Option<std::string> path = VirtualFileSystem::get()->to_native_path(virtual_path);
	REQUIRE(path.has_value());
	return *path;
}

struct FontFolder {
	std::filesystem::path root;

	FontFolder() {
		test_vfs();
		root = std::filesystem::temp_directory_path() /
			("aquila_font_tests_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(root / "fonts");
		REQUIRE(VirtualFileSystem::get()->mount(
			std::string(K_MOUNT), std::make_shared<Platform::Filesystem::NativeFileSystem>(root.string())));
	}

	~FontFolder() {
		VirtualFileSystem::get()->unmount(std::string(K_MOUNT));
		std::filesystem::remove_all(root);
	}

	AQUILA_NONCOPYABLE(FontFolder);
	AQUILA_NONMOVEABLE(FontFolder);

	[[nodiscard]] static std::string fonts() { return std::string(K_MOUNT) + "/fonts"; }
};

}

TEST_SUITE("Fonts") {
	TEST_CASE("the family name comes from inside the font file") {
		test_vfs();
		const std::vector<Uint8> geist = read_bytes("/resources/Engine/Fonts/Geist/Geist-Regular.ttf");
		CHECK(Text::FontFace::read_family_name(geist.data(), geist.size()) == std::string("Geist"));
		const std::vector<Uint8> mono = read_bytes("/resources/Engine/Fonts/Inconsolata/Inconsolata-Regular.ttf");
		CHECK(Text::FontFace::read_family_name(mono.data(), mono.size()) == std::string("Inconsolata"));
	}

	TEST_CASE("a file that isn't a font has no family and makes no face") {
		const std::string text = "this is not a font, just some text that is long enough";
		const auto *bytes = reinterpret_cast<const Uint8 *>(text.data());
		CHECK_FALSE(Text::FontFace::read_family_name(bytes, text.size()).has_value());
		CHECK(Text::FontFace::create(bytes, text.size()) == nullptr);
	}

	TEST_CASE("an added font folder lists its families and wins over the engine's") {
		FontFolder folder;
		FontManager &fonts = FontManager::get();
		const Option<std::string> engine_file = fonts.family_file("Inconsolata");
		REQUIRE(engine_file.has_value());

		Foundation::Registration added = fonts.add_font_dir(FontFolder::fonts());
		const Option<std::string> family = FontManager::import_font(
			native("/resources/Engine/Fonts/Inconsolata/Inconsolata-Regular.ttf"), FontFolder::fonts());
		REQUIRE(family == std::string("Inconsolata"));
		CHECK(std::filesystem::exists(folder.root / "fonts" / "Inconsolata" / "Inconsolata-Regular.ttf"));
		CHECK(fonts.family_file("Inconsolata") == FontFolder::fonts() + "/Inconsolata/Inconsolata-Regular.ttf");

		const std::vector<std::string> families = fonts.available_families();
		CHECK(std::ranges::count(families, "Inconsolata") == 1);
		CHECK(std::ranges::is_sorted(families));

		added.reset();
		CHECK(fonts.family_file("Inconsolata") == engine_file);
	}

	TEST_CASE("importing a file that isn't a font copies nothing") {
		FontFolder folder;
		const std::filesystem::path junk = folder.root / "Junk.ttf";
		std::ofstream(junk) << "definitely not a font file, but it has the right extension";

		CHECK_FALSE(FontManager::import_font(junk.string(), FontFolder::fonts()).has_value());
		CHECK(std::filesystem::is_empty(folder.root / "fonts"));
	}

	TEST_CASE("a family that no folder has has no file") {
		test_vfs();
		CHECK_FALSE(FontManager::get().family_file("No Such Family").has_value());
		CHECK_FALSE(FontManager::get().family_file("../Geist").has_value());
	}
}
