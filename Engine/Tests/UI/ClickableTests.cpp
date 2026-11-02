#include <doctest.h>

#include "Aquila/UI/Core/DragState.h"
#include "Aquila/UI/Widgets/Clickable.h"
#include "CanvasTestSupport.h"

using namespace Aquila::UI::Core;
using Aquila::Platform::KeyCode;

TEST_SUITE("Clickable") {
	TEST_CASE("a clickable is no drag source until it is made draggable") {
		Clickable clickable;
		CHECK_FALSE(clickable.is_draggable());
		clickable.set_draggable(true);
		CHECK(clickable.is_draggable());
		clickable.set_draggable(false);
		CHECK_FALSE(clickable.is_draggable());
	}

	TEST_CASE("the drag's payload comes from whoever listens") {
		Clickable clickable;
		clickable.set_draggable(true);
		clickable.on_drag_start_requested.connect([](DragState &state) {
			state.payload = std::string("/project/Content/Floor.aqmat");
			state.label = "Floor";
		});
		DragState state;
		clickable.on_drag_start(state);
		const auto *payload = std::any_cast<std::string>(&state.payload);
		REQUIRE(payload != nullptr);
		CHECK(*payload == "/project/Content/Floor.aqmat");
		CHECK(state.label == "Floor");
	}

	TEST_CASE("keys pressed on it are reported") {
		Clickable clickable;
		std::vector<KeyCode> keys;
		clickable.on_key_pressed.connect([&keys](KeyCode key, int) { keys.push_back(key); });
		clickable.on_key_press(KeyCode::F2, 0);
		clickable.on_key_press(KeyCode::Delete, 0);
		CHECK(keys == std::vector<KeyCode>{ KeyCode::F2, KeyCode::Delete });
	}
}
