#include <doctest.h>

#include "LoaderTestSupport.h"

#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/MessageBox.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/TextInput.h"

using namespace Aquila::UI::Core;

namespace {

bool has_class(const View &view, std::string_view name) {
	return std::ranges::find(view.get_classes(), name) != view.get_classes().end();
}

const Label *find_label(const View &view, std::string_view cls) {
	if (const auto *label = dynamic_cast<const Label *>(&view); label != nullptr && has_class(view, cls)) {
		return label;
	}
	for (const auto &child : view.get_children()) {
		if (const Label *found = find_label(*child, cls)) {
			return found;
		}
	}
	return nullptr;
}

std::vector<const View *> messages_in(const PropertyGrid &grid) {
	std::vector<const View *> found;
	for (const auto &child : grid.get_children()) {
		if (has_class(*child, "property-message")) {
			found.push_back(child.get());
		}
	}
	return found;
}

Usize index_of(const View &parent, const View *child) {
	const auto &children = parent.get_children();
	for (Usize i = 0; i < children.size(); ++i) {
		if (children[i].get() == child) {
			return i;
		}
	}
	return children.size();
}
}

TEST_SUITE("MessageBox") {
	TEST_CASE("each kind builds its box with the text") {
		LoaderCanvas fixture;
		View *error = create_message_box(fixture.host(), MessageKind::Error, "Missing reference");
		View *warning = create_message_box(fixture.host(), MessageKind::Warning, "Careful");
		View *info = create_message_box(fixture.host(), MessageKind::Info, "Hello");
		REQUIRE(error != nullptr);
		REQUIRE(warning != nullptr);
		REQUIRE(info != nullptr);

		CHECK(has_class(*error, "message-box"));
		CHECK(has_class(*error, "message-box-error"));
		CHECK(has_class(*warning, "message-box-warning"));
		CHECK(has_class(*info, "message-box-info"));
		const Label *text = find_label(*error, "message-box-text");
		REQUIRE(text != nullptr);
		CHECK(text->get_text() == "Missing reference");
	}

	TEST_CASE("outside a canvas nothing is built") {
		View loose;
		CHECK(create_message_box(loose, MessageKind::Error, "nope") == nullptr);
	}

	TEST_CASE("a grid puts a message right above the row and keeps one per row") {
		LoaderCanvas fixture;
		auto &grid = *fixture.host().add_child<PropertyGrid>();
		grid.add_row<TextInput>("First");
		auto *second = grid.add_row<TextInput>("Second");
		REQUIRE(second != nullptr);
		const View *second_row = second->get_parent();

		grid.set_message(*second, MessageKind::Error, "Missing reference");
		REQUIRE(messages_in(grid).size() == 1);
		const View *message = messages_in(grid).front();
		CHECK(index_of(grid, message) + 1 == index_of(grid, second_row));

		grid.set_message(*second, MessageKind::Warning, "Still missing");
		REQUIRE(messages_in(grid).size() == 1);
		CHECK(has_class(*messages_in(grid).front(), "message-box-warning"));

		grid.set_message_hidden(*second, true);
		CHECK(has_class(*messages_in(grid).front(), "hidden"));

		grid.clear_message(*second);
		CHECK(messages_in(grid).empty());
	}

	TEST_CASE("a message for a hidden row starts hidden") {
		LoaderCanvas fixture;
		auto &grid = *fixture.host().add_child<PropertyGrid>();
		auto *field = grid.add_row<TextInput>("Field");
		REQUIRE(field != nullptr);
		field->get_parent()->set_hidden(true);

		grid.set_message(*field, MessageKind::Error, "Missing reference");
		REQUIRE(messages_in(grid).size() == 1);
		CHECK(has_class(*messages_in(grid).front(), "hidden"));
	}
}
