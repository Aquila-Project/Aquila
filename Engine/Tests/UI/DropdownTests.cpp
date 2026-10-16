#include <doctest.h>

#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Dropdown.h"

using namespace Aquila::UI::Core;

namespace {

std::array<char, 3> g_texture_storage{};

Aquila::GFX::GfxTexture *fake_texture(int index) {
	return reinterpret_cast<Aquila::GFX::GfxTexture *>(&g_texture_storage.at(index));
}

Aquila::GFX::GfxTexture *header_icon(const Dropdown &dropdown) {
	const auto *header = dynamic_cast<const Button *>(dropdown.get_children().front().get());
	REQUIRE(header != nullptr);
	REQUIRE(header->get_content() != nullptr);
	return header->get_content()->get_icon()->get_texture();
}

}

TEST_SUITE("Dropdown") {
	TEST_CASE("the header shows the icon of the selected option") {
		Dropdown dropdown;
		dropdown.add_option("cube", "Cube", fake_texture(0));
		dropdown.add_option("sphere", "Sphere", fake_texture(1));

		dropdown.set_value("sphere");
		CHECK(header_icon(dropdown) == fake_texture(1));

		dropdown.on_changed.connect([](const std::string &) {});
		dropdown.set_value("cube");
		CHECK(header_icon(dropdown) == fake_texture(0));
	}

	TEST_CASE("the dropdown icon is used when nothing with an icon is selected") {
		Dropdown dropdown;
		dropdown.set_icon(fake_texture(2));
		dropdown.add_option("custom", "Custom");
		CHECK(header_icon(dropdown) == fake_texture(2));

		dropdown.add_option("cube", "Cube", fake_texture(0));
		dropdown.set_value("custom");
		CHECK(header_icon(dropdown) == fake_texture(2));

		dropdown.set_value("cube");
		CHECK(header_icon(dropdown) == fake_texture(0));

		dropdown.clear_selection();
		CHECK(header_icon(dropdown) == fake_texture(2));
	}

	TEST_CASE("options without icons leave the header without one") {
		Dropdown dropdown;
		dropdown.add_option("a", "A");
		dropdown.set_value("a");
		CHECK(header_icon(dropdown) == nullptr);
	}
}
