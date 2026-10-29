#include <doctest.h>

#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "GpuTestSupport.h"
#include "LoaderTestSupport.h"

using namespace Aquila::UI;
using namespace Aquila::UI::Core;

namespace {

constexpr Vec2 K_CANVAS = { 400.F, 300.F };

struct MenuStage {
	LoaderCanvas fixture{ static_cast<Uint32>(K_CANVAS.x), static_cast<Uint32>(K_CANVAS.y) };
	PopupMenu *menu = nullptr;

	MenuStage() {
		test_gpu();
		StyleProperties style;
		style.font_family = "regular";
		fixture.host().merge_style(style);
		menu = fixture.host().add_child<PopupMenu>();
	}
};

bool inside_canvas(const Rect &rect) {
	constexpr F32 K_SLACK = 0.5F;
	return rect.position.x >= -K_SLACK && rect.position.y >= -K_SLACK &&
		rect.position.x + rect.size.x <= K_CANVAS.x + K_SLACK && rect.position.y + rect.size.y <= K_CANVAS.y + K_SLACK;
}

Button *first_item(const PopupMenu &menu) {
	for (const auto &child : menu.get_children()) {
		if (auto *button = dynamic_cast<Button *>(child.get())) {
			return button;
		}
	}
	return nullptr;
}

}

TEST_SUITE("PopupMenu") {
	TEST_CASE("a menu that fits opens where it was asked") {
		MenuStage stage;
		stage.menu->add_item("Cube", [] {});
		stage.menu->open_at({ 20.F, 30.F });
		stage.fixture.settle();

		const Rect rect = stage.menu->get_absolute_rect();
		CHECK(rect.position.x == doctest::Approx(20.F));
		CHECK(rect.position.y == doctest::Approx(30.F));
	}

	TEST_CASE("a menu opened near the corner stays inside the canvas") {
		MenuStage stage;
		for (int i = 0; i < 6; ++i) {
			stage.menu->add_item("Component " + std::to_string(i), [] {});
		}
		stage.menu->open_at({ 390.F, 290.F });
		stage.fixture.settle();

		const Rect rect = stage.menu->get_absolute_rect();
		REQUIRE(rect.size.x > 100.F);
		REQUIRE(rect.size.y > 100.F);
		CHECK_MESSAGE(inside_canvas(rect), rect.position.x, ",", rect.position.y, " ", rect.size.x, "x", rect.size.y);
	}

	TEST_CASE("a submenu that doesn't fit on the right opens left of its menu") {
		MenuStage stage;
		PopupMenu *submenu = stage.menu->add_submenu("Rendering");
		submenu->add_item("Mesh", [] {});
		submenu->add_item("Material", [] {});
		stage.menu->open_at({ 230.F, 20.F });
		stage.fixture.settle();
		Button *item = first_item(*stage.menu);
		REQUIRE(item != nullptr);
		item->on_click();
		stage.fixture.settle();

		const Rect menu = stage.menu->get_absolute_rect();
		const Rect sub = submenu->get_absolute_rect();
		REQUIRE(sub.size.x > 0.F);
		CHECK(menu.position.x + menu.size.x <= K_CANVAS.x);
		CHECK(sub.position.x + sub.size.x <= menu.position.x + 0.5F);
		CHECK(inside_canvas(sub));
	}
}
