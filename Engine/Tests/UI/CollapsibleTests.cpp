#include <doctest.h>

#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Collapsible.h"

#include "CanvasTestSupport.h"

#include "Aquila/UI/Core/Canvas.h"

using namespace Aquila::UI::Core;

namespace {

bool has_class(const View &view, const char *name) {
	const auto &classes = view.get_classes();
	return std::find(classes.begin(), classes.end(), name) != classes.end();
}

} // namespace

TEST_SUITE("Collapsible") {
	TEST_CASE("header actions run their callback when clicked") {
		Collapsible collapsible("Card");
		int clicks = 0;
		Button *action = collapsible.add_header_action(nullptr, "Do it", [&clicks] { ++clicks; });
		REQUIRE(action != nullptr);

		action->on_click();
		action->on_click();

		CHECK(clicks == 2);
	}

	TEST_CASE("the collapsed class follows the expanded state") {
		Collapsible collapsible("Card");
		CHECK_FALSE(has_class(collapsible, "collapsed"));

		collapsible.set_expanded(false);
		CHECK(has_class(collapsible, "collapsed"));

		collapsible.set_expanded(true);
		CHECK_FALSE(has_class(collapsible, "collapsed"));
	}

	TEST_CASE("a variant tags the collapsible and its parts") {
		Collapsible collapsible("Card");
		collapsible.set_variant("card");

		CHECK(has_class(collapsible, "card"));
		const auto &header = *collapsible.get_children().front();
		CHECK(has_class(header, "card-header"));
	}

	TEST_CASE("dragging pins the size while floating and restores the original style afterwards") {
		CanvasSingletonsScope singletons;
		Canvas canvas(400, 400);

		View *rail = canvas.get_root()->add_child<View>();
		Aquila::UI::StyleProperties rail_style;
		rail_style.width = Aquila::UI::StyleLength::pixel(200.F);
		rail_style.height = Aquila::UI::StyleLength::pixel(300.F);
		rail_style.flex_direction = Aquila::UI::FlexDirection::Column;
		rail->merge_style(rail_style);

		auto *card = rail->add_child<Collapsible>(std::string("Card"));
		Aquila::UI::StyleProperties grow;
		grow.width = Aquila::UI::StyleLength::grow();
		grow.height = Aquila::UI::StyleLength::grow();
		card->set_style(grow);
		rail->add_child<Collapsible>(std::string("Other"));

		for (int i = 0; i < 3; ++i) {
			canvas.update(0.016F);
			canvas.compute();
		}

		card->begin_drag();
		REQUIRE(card->get_style().height.has_value());
		CHECK(card->get_style().height->unit == Aquila::UI::LengthUnit::Pixel);
		CHECK(card->get_style().height->value > 0.F);

		card->on_update(0.016F);
		REQUIRE(card->get_style().height.has_value());
		CHECK(card->get_style().height->unit == Aquila::UI::LengthUnit::Grow);
		CHECK(card->get_style().width->unit == Aquila::UI::LengthUnit::Grow);
	}

	TEST_CASE("a collapsible that is not reorderable cannot be dragged") {
		CanvasSingletonsScope singletons;
		Canvas canvas(400, 400);
		View *rail = canvas.get_root()->add_child<View>();
		auto *card = rail->add_child<Collapsible>(std::string("Card"));
		card->set_reorderable(false);

		card->begin_drag();

		CHECK_FALSE(card->has_floating());
		CHECK(rail->get_children().size() == 1);
	}

	TEST_CASE("the reorderable attribute can be set from a layout") {
		Collapsible card("Card");
		card.apply_xml_attribute("reorderable", "false");
		CHECK_FALSE(card.is_reorderable());
	}

	TEST_CASE("a collapsible that is not collapsible ignores header clicks") {
		CanvasSingletonsScope singletons;
		Canvas canvas(400, 400);
		auto *card = canvas.get_root()->add_child<Collapsible>(std::string("Card"));
		card->set_collapsible(false);

		auto *title = dynamic_cast<Button *>(card->View::get_children()[0]->get_children()[0].get());
		REQUIRE(title != nullptr);
		title->on_click();

		CHECK(card->is_expanded());
	}

	TEST_CASE("the collapsible attribute can be set from a layout") {
		Collapsible card("Card");
		card.apply_xml_attribute("collapsible", "false");
		CHECK_FALSE(card.is_collapsible());
	}

	TEST_CASE("a leading chevron sits before the title and toggles the section") {
		Collapsible card("Card");
		card.set_variant("panel");
		card.set_state_icons(nullptr, nullptr, true);

		const View &header = *card.View::get_children()[0];
		auto *chevron = dynamic_cast<Button *>(header.get_children()[0].get());
		REQUIRE(chevron != nullptr);
		CHECK(has_class(*chevron, "panel-chevron"));

		chevron->on_click();
		CHECK_FALSE(card.is_expanded());
	}
}
