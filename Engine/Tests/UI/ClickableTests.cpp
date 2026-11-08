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

	TEST_CASE("a clickable is no drop target until it accepts drops") {
		Clickable clickable;
		View *child = clickable.add_child(std::make_unique<View>());
		CHECK_FALSE(clickable.is_accepting_payload());
		CHECK(child->get_first_parent_that_accepts_drop() == nullptr);
		clickable.set_accepts_drop(true);
		CHECK(clickable.is_accepting_payload());
		CHECK(child->get_first_parent_that_accepts_drop() == &clickable);
		clickable.set_accepts_drop(false);
		CHECK_FALSE(clickable.is_accepting_payload());
	}

	TEST_CASE("drags over it and drops on it are reported with their payload") {
		Clickable clickable;
		clickable.set_accepts_drop(true);
		std::vector<std::string> calls;
		const auto record = [&calls](std::string name) {
			return [&calls, name = std::move(name)](DragState &state) {
				const auto *payload = std::any_cast<std::string>(&state.payload);
				calls.push_back(name + " " + (payload != nullptr ? *payload : "?"));
			};
		};
		clickable.on_drag_entered.connect(record("enter"));
		clickable.on_dropped.connect(record("drop"));
		clickable.on_drag_left.connect(record("leave"));
		DragState state{ .payload = std::string("a.png"), .label = "a", .is_dragging = true };
		clickable.on_drag_enter(state);
		clickable.on_drop(state);
		clickable.on_drag_leave(state);
		CHECK(calls == std::vector<std::string>{ "enter a.png", "drop a.png", "leave a.png" });
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
