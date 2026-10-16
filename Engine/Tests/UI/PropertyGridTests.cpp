#include <doctest.h>

#include "Aquila/UI/Widgets/Checkbox.h"
#include "Aquila/UI/Widgets/DragFloat.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/VecField.h"

using namespace Aquila::UI::Core;

namespace {

bool has_class(const View &view, const char *name) {
	const auto &classes = view.get_classes();
	return std::find(classes.begin(), classes.end(), name) != classes.end();
}

} // namespace

TEST_SUITE("PropertyGrid") {
	TEST_CASE("rows put the label beside the widget by default") {
		PropertyGrid grid;
		auto *field = grid.add_row<DragFloat>("Speed");

		const View *row = field->get_parent();
		CHECK(has_class(*row, "property-row"));
		CHECK_FALSE(has_class(*row, "property-row-stacked"));
	}

	TEST_CASE("stacked rows put the label above the widget") {
		PropertyGrid grid;
		auto *field = grid.add_stacked_row<Vec3Field>("Position");

		const View *row = field->get_parent();
		CHECK(has_class(*row, "property-row-stacked"));
		const View &label = *row->get_children().front();
		CHECK(has_class(label, "property-label-stacked"));
	}

	TEST_CASE("split rows keep the label in its own column and tag the widget by type") {
		PropertyGrid grid;
		grid.set_split(true);
		auto *field = grid.add_row<DragFloat>("Speed");

		const View *row = field->get_parent();
		CHECK(has_class(*row, "property-split-row"));
		CHECK(has_class(*row->get_children().front(), "property-split-label"));
		CHECK(has_class(*field, "split-DragFloat"));
	}

	TEST_CASE("split stacked rows align the label with the first line of the widget") {
		PropertyGrid grid;
		grid.set_split(true);
		auto *field = grid.add_stacked_row<Vec3Field>("Location");

		CHECK(has_class(*field->get_parent(), "property-split-row-top"));
	}

	TEST_CASE("split check rows put the text after the checkbox and leave the label column empty") {
		PropertyGrid grid;
		grid.set_split(true);
		auto *check = grid.add_check_row<Checkbox>("Cast Shadows", true);

		const auto &cells = check->get_parent()->get_children();
		REQUIRE(cells.size() == 3);
		CHECK(dynamic_cast<Label *>(cells[0].get())->get_text().empty());
		CHECK(cells[1].get() == check);
		CHECK(dynamic_cast<Label *>(cells[2].get())->get_text() == "Cast Shadows");
	}

	TEST_CASE("check rows fall back to a regular row outside split mode") {
		PropertyGrid grid;
		auto *check = grid.add_check_row<Checkbox>("Cast Shadows", true);
		CHECK(has_class(*check->get_parent(), "property-row"));
	}

	TEST_CASE("stacked vectors label each axis inside its field") {
		Vec3Field field;
		field.set_stacked(true);

		CHECK(has_class(field, "vec-field-stacked"));
		CHECK(has_class(*field.get_children()[0], "vec-stack-first"));
		CHECK(has_class(*field.get_children()[1], "vec-stack-middle"));
		CHECK(has_class(*field.get_children()[2], "vec-stack-last"));
	}
}
