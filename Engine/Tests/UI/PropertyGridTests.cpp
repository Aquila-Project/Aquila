#include <doctest.h>

#include "LoaderTestSupport.h"

#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Checkbox.h"
#include "Aquila/UI/Widgets/DragFloat.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Aquila/UI/Widgets/VecField.h"

using namespace Aquila::UI::Core;

namespace {

struct CanvasGrid {
	LoaderCanvas fixture;
	PropertyGrid &grid = *fixture.host().add_child<PropertyGrid>();
};

bool has_class(const View &view, const char *name) {
	const auto &classes = view.get_classes();
	return std::find(classes.begin(), classes.end(), name) != classes.end();
}

} // namespace

TEST_SUITE("PropertyGrid") {
	TEST_CASE("rows put the label beside the widget by default") {
		CanvasGrid in_canvas;
		PropertyGrid &grid = in_canvas.grid;
		auto *field = grid.add_row<DragFloat>("Speed");

		const View *row = field->get_parent();
		CHECK(has_class(*row, "property-row"));
		CHECK_FALSE(has_class(*row, "property-row-stacked"));
	}

	TEST_CASE("stacked rows put the label above the widget") {
		CanvasGrid in_canvas;
		PropertyGrid &grid = in_canvas.grid;
		auto *field = grid.add_stacked_row<Vec3Field>("Position");

		const View *row = field->get_parent();
		CHECK(has_class(*row, "property-row-stacked"));
		const View &label = *row->get_children().front();
		CHECK(has_class(label, "property-label-stacked"));
	}

	TEST_CASE("split rows keep the label in its own column and tag the widget by type") {
		CanvasGrid in_canvas;
		PropertyGrid &grid = in_canvas.grid;
		grid.set_split(true);
		auto *field = grid.add_row<DragFloat>("Speed");

		const View *row = field->get_parent();
		CHECK(has_class(*row, "property-split-row"));
		CHECK(has_class(*row->get_children().front(), "property-split-label"));
		CHECK(has_class(*field, "split-DragFloat"));
	}

	TEST_CASE("split stacked rows align the label with the first line of the widget") {
		CanvasGrid in_canvas;
		PropertyGrid &grid = in_canvas.grid;
		grid.set_split(true);
		auto *field = grid.add_stacked_row<Vec3Field>("Location");

		CHECK(has_class(*field->get_parent(), "property-split-row-top"));
	}

	TEST_CASE("split check rows put the text after the checkbox and leave the label column empty") {
		CanvasGrid in_canvas;
		PropertyGrid &grid = in_canvas.grid;
		grid.set_split(true);
		auto *check = grid.add_check_row<Checkbox>("Cast Shadows", true);

		const auto &cells = check->get_parent()->get_children();
		REQUIRE(cells.size() == 3);
		CHECK(dynamic_cast<Label *>(cells[0].get())->get_text().empty());
		CHECK(cells[1].get() == check);
		CHECK(dynamic_cast<Label *>(cells[2].get())->get_text() == "Cast Shadows");
	}

	TEST_CASE("check rows fall back to a regular row outside split mode") {
		CanvasGrid in_canvas;
		PropertyGrid &grid = in_canvas.grid;
		auto *check = grid.add_check_row<Checkbox>("Cast Shadows", true);
		CHECK(has_class(*check->get_parent(), "property-row"));
	}

	TEST_CASE("a grid outside a canvas reports it and adds nothing") {
		PropertyGrid grid;
		LogCapture capture;
		CHECK(grid.add_row<DragFloat>("Speed") == nullptr);
		CHECK(grid.get_children().empty());
		CHECK_MESSAGE(capture.stream.str().find("PropertyGrid: 'Speed' needs a canvas with a layout loader") !=
						  std::string::npos,
					  capture.stream.str());
	}

	TEST_CASE("labels show as written") {
		CanvasGrid in_canvas;
		auto *field = in_canvas.grid.add_row<DragFloat>("Cost $x & <b>");
		REQUIRE(field != nullptr);
		const auto *label = dynamic_cast<const Label *>(field->get_parent()->get_children().front().get());
		REQUIRE(label != nullptr);
		CHECK(label->get_text() == "Cost $x & <b>");
	}

	TEST_CASE("inline labels take the grid's label width") {
		LoaderCanvas fixture;
		auto *grid = fixture.host().add_child<PropertyGrid>(150.F);
		auto *field = grid->add_row<DragFloat>("Speed");
		REQUIRE(field != nullptr);
		fixture.settle();
		const View &label = *field->get_parent()->get_children().front();
		CHECK(label.get_computed_style().width == Aquila::UI::StyleLength::pixel(150.F));
	}

	TEST_CASE("a nested grid keeps the label width and isn't split") {
		LoaderCanvas fixture;
		auto *grid = fixture.host().add_child<PropertyGrid>(130.F);
		grid->set_split(true);
		PropertyGrid *nested = grid->add_grid();
		REQUIRE(nested != nullptr);
		CHECK(nested->get_parent() == grid);
		CHECK(nested->get_label_width() == 130.F);
		CHECK_FALSE(nested->is_split());
	}

	TEST_CASE("an action goes after its widget in the row") {
		LoaderCanvas fixture;
		auto *grid = fixture.host().add_child<PropertyGrid>();
		auto *other = fixture.host().add_child<PropertyGrid>();
		auto *field = grid->add_row<TextInput>("Output");
		auto *foreign = other->add_row<TextInput>("Elsewhere");
		REQUIRE(field != nullptr);
		REQUIRE(foreign != nullptr);
		auto browse = std::make_unique<Button>();
		Button *raw = browse.get();
		CHECK(grid->add_action(*field, std::move(browse)) == raw);
		const auto &cells = field->get_parent()->get_children();
		REQUIRE(cells.size() == 3);
		CHECK(cells[1].get() == field);
		CHECK(cells[2].get() == raw);
		CHECK(grid->add_action(*foreign, std::make_unique<Button>()) == nullptr);
		CHECK(foreign->get_parent()->get_children().size() == 2);
	}

	TEST_CASE("label-width from markup sets the label width") {
		LoaderCanvas fixture;
		const Unique<View> loaded = fixture.loader.load_string(R"(<PropertyGrid label-width="120"/>)");
		const auto *grid = dynamic_cast<const PropertyGrid *>(loaded.get());
		REQUIRE(grid != nullptr);
		CHECK(grid->get_label_width() == 120.F);
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
