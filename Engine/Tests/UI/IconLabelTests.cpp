#include <doctest.h>

#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Widgets/IconLabel.h"
#include "CanvasTestSupport.h"

using namespace Aquila::UI;
using namespace Aquila::UI::Core;

namespace {

Display label_display(Canvas &canvas, const IconLabel &icon_label) {
	canvas.update(0.016F);
	canvas.compute();
	return icon_label.get_children()[1]->get_computed_style().display;
}

struct HiddenCanvas {
	CanvasSingletonsScope singletons;
	Canvas canvas{ 200, 100 };
	IconLabel *icon_label = nullptr;

	HiddenCanvas() {
		StyleParser::LoadString(".hidden { display: none; }", canvas.get_style_sheet());
		icon_label = canvas.get_root()->add_child<IconLabel>();
	}
};

} // namespace

TEST_SUITE("IconLabel") {
	TEST_CASE("an empty label takes no space so an icon can be centered") {
		HiddenCanvas fixture;
		CHECK(label_display(fixture.canvas, *fixture.icon_label) == Display::None);
	}

	TEST_CASE("the label is shown once it has text") {
		HiddenCanvas fixture;
		fixture.icon_label->set_text("Move");
		CHECK(label_display(fixture.canvas, *fixture.icon_label) == Display::Flex);

		fixture.icon_label->set_text("");
		CHECK(label_display(fixture.canvas, *fixture.icon_label) == Display::None);
	}
}
