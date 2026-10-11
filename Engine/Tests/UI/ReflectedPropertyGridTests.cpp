#include <doctest.h>

#include "Aquila/UI/Widgets/AssetSlot.h"
#include "Aquila/UI/Widgets/Checkbox.h"
#include "Aquila/UI/Widgets/DragFloat.h"
#include "Aquila/UI/Widgets/DragInt.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/ReflectedPropertyGrid.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Aquila/UI/Widgets/Toggle.h"
#include "Aquila/UI/Widgets/VecField.h"

#include <algorithm>

using namespace Aquila::UI::Core;
using namespace Aquila::Reflection;

namespace {

enum class Shape { Box, Ball };

struct Thing {
	float size = 2.F;
	int count = 3;
	bool solid = true;
	bool glowing = false;
	Vec3 offset{ 1.F, 2.F, 3.F };
	std::string label = "thing";
	Shape shape = Shape::Box;
	float radius = 0.5F;

	int radius_writes = 0;
	void set_radius(float value) {
		radius = value;
		++radius_writes;
	}
	float get_radius() const { return radius; }
};

TypeInfo make_type() {
	TypeInfo info("Thing");
	TypeBuilder<Thing>(info)
		.property("Size", &Thing::size, { .min = 0.F, .max = 10.F, .slider = true })
		.property("Count", &Thing::count)
		.property("Solid", &Thing::solid)
		.property("Glowing", &Thing::glowing, { .toggle = true })
		.property("Offset", &Thing::offset)
		.property("Label", &Thing::label)
		.property("Shape", &Thing::shape)
		.options({ { "Box", 0 }, { "Ball", 1 } })
		.property("Radius", &Thing::get_radius, &Thing::set_radius)
		.visible_if([](const Thing &t) { return t.shape == Shape::Ball; });
	return info;
}

bool is_hidden(const View *view) {
	const auto &classes = view->get_classes();
	return std::find(classes.begin(), classes.end(), "hidden") != classes.end();
}

template <typename W> W &widget(ReflectedPropertyGrid &grid, const char *name) {
	auto *found = dynamic_cast<W *>(grid.get_widget(name));
	REQUIRE(found != nullptr);
	return *found;
}

}

TEST_SUITE("ReflectedPropertyGrid") {
	TEST_CASE("each property kind gets its matching widget") {
		const TypeInfo type = make_type();
		PropertyGrid grid;
		ReflectedPropertyGrid reflected(grid, type, nullptr);

		CHECK(dynamic_cast<DragFloat *>(reflected.get_widget("Size")) != nullptr);
		CHECK(dynamic_cast<DragInt *>(reflected.get_widget("Count")) != nullptr);
		CHECK(dynamic_cast<Checkbox *>(reflected.get_widget("Solid")) != nullptr);
		CHECK(dynamic_cast<Toggle *>(reflected.get_widget("Glowing")) != nullptr);
		CHECK(dynamic_cast<Vec3Field *>(reflected.get_widget("Offset")) != nullptr);
		CHECK(dynamic_cast<TextInput *>(reflected.get_widget("Label")) != nullptr);
		CHECK(dynamic_cast<Dropdown *>(reflected.get_widget("Shape")) != nullptr);
		CHECK(reflected.get_widget("Nope") == nullptr);
	}

	TEST_CASE("bind pulls the instance's values into the widgets") {
		const TypeInfo type = make_type();
		PropertyGrid grid;
		ReflectedPropertyGrid reflected(grid, type, nullptr);
		Thing thing;
		reflected.bind(&thing);

		CHECK(widget<DragFloat>(reflected, "Size").get_value() == doctest::Approx(2.F));
		CHECK(widget<DragInt>(reflected, "Count").get_int_value() == 3);
		CHECK(widget<Checkbox>(reflected, "Solid").get_value());
		CHECK_FALSE(widget<Toggle>(reflected, "Glowing").get_value());
		CHECK(widget<Vec3Field>(reflected, "Offset").get_value().z == doctest::Approx(3.F));
		CHECK(widget<TextInput>(reflected, "Label").get_text() == "thing");
		CHECK(widget<Dropdown>(reflected, "Shape").get_value() == "0");
	}

	TEST_CASE("binding does not write back to the instance") {
		const TypeInfo type = make_type();
		PropertyGrid grid;
		ReflectedPropertyGrid reflected(grid, type, nullptr);
		Thing thing;
		thing.shape = Shape::Ball;
		reflected.bind(&thing);

		CHECK(thing.radius_writes == 0);
	}

	TEST_CASE("user edits are written through the property and reported") {
		const TypeInfo type = make_type();
		PropertyGrid grid;
		ReflectedPropertyGrid reflected(grid, type, nullptr);
		Thing thing;
		int edits = 0;
		reflected.bind(&thing, [&edits] { ++edits; });

		widget<DragFloat>(reflected, "Size").on_changed(6.F);
		widget<DragInt>(reflected, "Count").on_changed(8.4F);
		widget<Checkbox>(reflected, "Solid").on_changed(false);
		widget<Vec3Field>(reflected, "Offset").on_changed(Vec3(9.F, 8.F, 7.F));
		widget<TextInput>(reflected, "Label").on_changed("renamed");
		widget<Dropdown>(reflected, "Shape").on_changed("1");

		CHECK(thing.size == doctest::Approx(6.F));
		CHECK(thing.count == 8);
		CHECK_FALSE(thing.solid);
		CHECK(thing.offset.x == doctest::Approx(9.F));
		CHECK(thing.label == "renamed");
		CHECK(thing.shape == Shape::Ball);
		CHECK(edits == 6);
	}

	TEST_CASE("visibility follows the instance, including after an edit") {
		const TypeInfo type = make_type();
		PropertyGrid grid;
		ReflectedPropertyGrid reflected(grid, type, nullptr);
		Thing thing;
		reflected.bind(&thing);

		View *radius_row = reflected.get_widget("Radius")->get_parent();
		CHECK(is_hidden(radius_row));

		widget<Dropdown>(reflected, "Shape").on_changed("1");
		CHECK_FALSE(is_hidden(radius_row));
	}

	TEST_CASE("rebinding to another instance switches what is edited") {
		const TypeInfo type = make_type();
		PropertyGrid grid;
		ReflectedPropertyGrid reflected(grid, type, nullptr);
		Thing first;
		Thing second;
		second.size = 9.F;

		reflected.bind(&first);
		reflected.bind(&second);
		CHECK(widget<DragFloat>(reflected, "Size").get_value() == doctest::Approx(9.F));

		widget<DragFloat>(reflected, "Size").on_changed(4.F);
		CHECK(second.size == doctest::Approx(4.F));
		CHECK(first.size == doctest::Approx(2.F));
	}

	TEST_CASE("edits are ignored once detached") {
		const TypeInfo type = make_type();
		PropertyGrid grid;
		ReflectedPropertyGrid reflected(grid, type, nullptr);
		Thing thing;
		reflected.bind(&thing);
		reflected.bind(nullptr);

		widget<DragFloat>(reflected, "Size").on_changed(5.F);
		CHECK(thing.size == doctest::Approx(2.F));
	}

	TEST_CASE("split mode uses checkboxes, stacked vectors and sliders") {
		const TypeInfo type = make_type();
		PropertyGrid grid;
		grid.set_split(true);
		ReflectedPropertyGrid reflected(grid, type, nullptr);

		CHECK(dynamic_cast<Checkbox *>(reflected.get_widget("Glowing")) != nullptr);
		const auto &offset_classes = reflected.get_widget("Offset")->get_classes();
		CHECK(std::ranges::find(offset_classes, "vec-field-stacked") != offset_classes.end());
		const auto &size_classes = reflected.get_widget("Size")->get_classes();
		CHECK(std::ranges::find(size_classes, "drag-float-slider") != size_classes.end());
	}

	TEST_CASE("split check rows still hide with their property") {
		const TypeInfo type = make_type();
		PropertyGrid grid;
		grid.set_split(true);
		ReflectedPropertyGrid reflected(grid, type, nullptr);
		Thing thing;
		reflected.bind(&thing);

		CHECK(is_hidden(reflected.get_widget("Radius")->get_parent()));
		widget<Checkbox>(reflected, "Glowing").on_changed(true);
		CHECK(thing.glowing);
	}

	TEST_CASE("read-only strings are shown as labels") {
		TypeInfo type("Thing");
		TypeBuilder<Thing>(type).read_only("Id", [](const Thing &t) { return t.label + "#1"; });
		PropertyGrid grid;
		ReflectedPropertyGrid reflected(grid, type, nullptr);
		Thing thing;
		reflected.bind(&thing);

		CHECK(widget<Label>(reflected, "Id").get_text() == "thing#1");
	}

	TEST_CASE("strings with an asset type are slots that take matching drops") {
		TypeInfo type("Thing");
		TypeBuilder<Thing>(type).property("Script", &Thing::label, { .asset_type = "script" });
		PropertyGrid grid;
		ReflectedPropertyGrid reflected(grid, type, nullptr);
		Thing thing;
		thing.label = "/scripts/old.luau";
		reflected.bind(&thing);

		auto &slot = widget<AssetSlot>(reflected, "Script");
		CHECK(slot.get_value().display_name == "old.luau");

		DragState drag{};
		drag.payload = AssetPayload{ "/textures/wood.png", "texture", "wood.png" };
		slot.on_drop(drag);
		CHECK(thing.label == "/scripts/old.luau");

		drag.payload = AssetPayload{ "/scripts/spin.luau", "script", "spin.luau" };
		slot.on_drop(drag);
		CHECK(thing.label == "/scripts/spin.luau");

		slot.clear();
		CHECK(thing.label.empty());
	}

	TEST_CASE("asset slots say what they accept") {
		AssetSlot slot("script");
		const auto &label = dynamic_cast<const Label &>(*slot.get_children().front());
		CHECK(label.get_text() == "Drop a script here");

		DragState drag{};
		drag.payload = AssetPayload{ "/textures/wood.png", "texture", "wood.png" };
		slot.on_drag_enter(drag);
		CHECK(label.get_text() == "Not a script");
		slot.on_drag_leave(drag);
		CHECK(label.get_text() == "Drop a script here");

		drag.payload = AssetPayload{ "/scripts/spin.luau", "script", "spin.luau" };
		slot.on_drag_enter(drag);
		CHECK(label.get_text() == "Drop to use spin.luau");

		slot.set_accepted_type("image");
		slot.on_drag_leave(drag);
		CHECK(label.get_text() == "Drop an image here");
	}
}
