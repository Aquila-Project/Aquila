#include <doctest.h>

#include "CanvasTestSupport.h"

#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Widgets/ScrollView.h"

using namespace Aquila::UI;
using namespace Aquila::UI::Core;

namespace {

void frame(Canvas &canvas) {
	canvas.update(0.016F);
	canvas.compute();
}

void frames(Canvas &canvas) {
	for (int i = 0; i < 4; ++i) {
		frame(canvas);
	}
}

void load_scroll_styles(Canvas &canvas) {
	StyleParser::LoadString("ScrollView { overflow: scroll; width: 200px; height: 100px; flex-direction: column; } "
							".scroll-inner { flex-direction: column; } .scroll-bar-track { height: 100%; } "
							".hidden { display: none; }",
							canvas.get_style_sheet());
}

View *add_block(View &parent, F32 height) {
	View *block = parent.add_child(std::make_unique<View>());
	StyleProperties style;
	style.width = StyleLength::pixel(100.F);
	style.height = StyleLength::pixel(height);
	block->set_style(style);
	return block;
}

struct TallScroll {
	explicit TallScroll(Canvas &canvas) {
		load_scroll_styles(canvas);
		scroll = canvas.get_root()->add_child<ScrollView>();
		content = add_block(*scroll, 500.F);
		frames(canvas);
	}

	ScrollView *scroll = nullptr;
	View *content = nullptr;
};

float bottom_of(const View &view) {
	const Rect rect = view.get_absolute_rect();
	return rect.position.y + rect.size.y;
}

View &track_of(const ScrollView &scroll) {
	return *scroll.get_children()[1];
}

View &thumb_of(const ScrollView &scroll) {
	return *track_of(scroll).get_children()[0];
}

bool scroll_bar_shown(const ScrollView &scroll) {
	const std::vector<std::string> &classes = track_of(scroll).get_classes();
	return std::ranges::find(classes, "hidden") == classes.end();
}

} // namespace

TEST_SUITE("ScrollView") {
	TEST_CASE("content starts at the top") {
		CanvasSingletonsScope singletons;
		Canvas canvas(400, 400);
		TallScroll tall(canvas);

		CHECK_FALSE(tall.scroll->is_at_bottom());
	}

	TEST_CASE("content that overflows from the start shows the scroll bar") {
		CanvasSingletonsScope singletons;
		Canvas canvas(400, 400);
		TallScroll tall(canvas);

		CHECK(scroll_bar_shown(*tall.scroll));
	}

	TEST_CASE("scroll_to_bottom shows the end of tall content") {
		CanvasSingletonsScope singletons;
		Canvas canvas(400, 400);
		TallScroll tall(canvas);

		tall.scroll->scroll_to_bottom();
		frames(canvas);

		CHECK(tall.scroll->is_at_bottom());
		CHECK(bottom_of(*tall.content) == doctest::Approx(bottom_of(*tall.scroll)).epsilon(0.01));
	}

	TEST_CASE("scrolling back up leaves the bottom") {
		CanvasSingletonsScope singletons;
		Canvas canvas(400, 400);
		TallScroll tall(canvas);

		tall.scroll->scroll_to_bottom();
		frames(canvas);
		tall.scroll->set_scroll_offset(0.F);
		frames(canvas);

		CHECK_FALSE(tall.scroll->is_at_bottom());
	}

	TEST_CASE("a hidden scroll view keeps its offset") {
		CanvasSingletonsScope singletons;
		Canvas canvas(400, 400);
		TallScroll tall(canvas);

		tall.scroll->scroll_to_bottom();
		frames(canvas);
		tall.scroll->set_hidden(true);
		frames(canvas);
		tall.scroll->set_hidden(false);
		frames(canvas);

		CHECK(tall.scroll->is_at_bottom());
		CHECK(bottom_of(*tall.content) == doctest::Approx(bottom_of(*tall.scroll)).epsilon(0.01));
	}

	TEST_CASE("wheel steps before a frame all count") {
		CanvasSingletonsScope singletons;
		Canvas canvas(400, 400);
		TallScroll tall(canvas);

		tall.scroll->on_scroll({ 0.F, -1.F });
		tall.scroll->on_scroll({ 0.F, -1.F });
		frame(canvas);

		CHECK(tall.scroll->get_scroll_offset() == doctest::Approx(80.F));
	}

	TEST_CASE("the scroll bar follows a scroll in the same frame") {
		CanvasSingletonsScope singletons;
		Canvas canvas(400, 400);
		TallScroll tall(canvas);

		tall.scroll->scroll_to_bottom();
		frame(canvas);

		CHECK(bottom_of(thumb_of(*tall.scroll)) == doctest::Approx(bottom_of(*tall.scroll)).epsilon(0.01));
	}

	TEST_CASE("the scroll bar follows a resize in the same frame") {
		CanvasSingletonsScope singletons;
		Canvas canvas(400, 400);
		TallScroll tall(canvas);

		StyleProperties taller;
		taller.height = StyleLength::pixel(250.F);
		tall.scroll->merge_style(taller);
		frame(canvas);

		CHECK(track_of(*tall.scroll).get_absolute_rect().size.y == doctest::Approx(250.F));
		CHECK(thumb_of(*tall.scroll).get_absolute_rect().size.y == doctest::Approx(125.F));
	}

	TEST_CASE("the thumb only drags with the left button") {
		CanvasSingletonsScope singletons;
		Canvas canvas(400, 400);
		TallScroll tall(canvas);
		View &thumb = thumb_of(*tall.scroll);
		const Vec2 grab = thumb.get_absolute_rect().position + Vec2(2.F, 2.F);

		thumb.on_mouse_press(Aquila::Platform::MouseButton::Right, grab);
		thumb.on_mouse_move(grab + Vec2(0.F, 38.F));
		thumb.on_mouse_release(Aquila::Platform::MouseButton::Right, grab + Vec2(0.F, 38.F));
		frames(canvas);
		CHECK(tall.scroll->get_scroll_offset() == doctest::Approx(0.F));

		thumb.on_mouse_press(Aquila::Platform::MouseButton::Left, grab);
		thumb.on_mouse_move(grab + Vec2(0.F, 38.F));
		frames(canvas);
		CHECK(tall.scroll->get_scroll_offset() == doctest::Approx(200.F));
	}

	TEST_CASE("scroll_into_view brings a row below the fold into view") {
		CanvasSingletonsScope singletons;
		Canvas canvas(400, 400);
		load_scroll_styles(canvas);
		auto *scroll = canvas.get_root()->add_child<ScrollView>();
		std::vector<View *> rows;
		for (int i = 0; i < 10; ++i) {
			rows.push_back(add_block(*scroll, 50.F));
		}
		frames(canvas);

		canvas.scroll_into_view(rows[7]);
		frames(canvas);

		CHECK(bottom_of(*rows[7]) == doctest::Approx(bottom_of(*scroll)).epsilon(0.01));
	}

	TEST_CASE("the scroll bar keeps following its content after the view moves") {
		CanvasSingletonsScope singletons;
		Canvas canvas(400, 400);
		TallScroll tall(canvas);
		StyleProperties height;
		height.height = StyleLength::pixel(50.F);
		tall.content->merge_style(height);
		frames(canvas);
		REQUIRE_FALSE(scroll_bar_shown(*tall.scroll));

		View *holder = canvas.get_root()->add_child(std::make_unique<View>());
		holder->add_child(canvas.get_root()->detach_child(tall.scroll));
		frames(canvas);

		height.height = StyleLength::pixel(500.F);
		tall.content->merge_style(height);
		frames(canvas);

		CHECK(scroll_bar_shown(*tall.scroll));
	}
}
