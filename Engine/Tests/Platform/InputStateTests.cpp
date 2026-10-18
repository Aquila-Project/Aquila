#include <doctest.h>

#include "Aquila/Platform/InputState.h"

using namespace Aquila;
using namespace Aquila::Platform;
using namespace Aquila::Platform::Events;

namespace {

void feed(InputState &state, Event &&event) {
	state.on_event(event);
}

}

TEST_SUITE("InputState") {
	TEST_CASE("keys are just pressed for one frame and held until released") {
		InputState state;
		feed(state, KeyPressedEvent(KeyCode::W, 0));

		CHECK(state.is_key_down(KeyCode::W));
		CHECK(state.is_key_just_pressed(KeyCode::W));
		CHECK_FALSE(state.is_key_just_released(KeyCode::W));

		state.end_frame();
		CHECK(state.is_key_down(KeyCode::W));
		CHECK_FALSE(state.is_key_just_pressed(KeyCode::W));

		feed(state, KeyReleasedEvent(KeyCode::W));
		CHECK_FALSE(state.is_key_down(KeyCode::W));
		CHECK(state.is_key_just_released(KeyCode::W));
	}

	TEST_CASE("key repeat doesn't count as a new press") {
		InputState state;
		feed(state, KeyPressedEvent(KeyCode::Space, 0));
		state.end_frame();
		feed(state, KeyPressedEvent(KeyCode::Space, 1));

		CHECK(state.is_key_down(KeyCode::Space));
		CHECK_FALSE(state.is_key_just_pressed(KeyCode::Space));
	}

	TEST_CASE("a tap inside one frame is both just pressed and just released") {
		InputState state;
		feed(state, MouseButtonPressedEvent(MouseButton::Left));
		feed(state, MouseButtonReleasedEvent(MouseButton::Left));

		CHECK_FALSE(state.is_mouse_button_down(MouseButton::Left));
		CHECK(state.is_mouse_button_just_pressed(MouseButton::Left));
		CHECK(state.is_mouse_button_just_released(MouseButton::Left));
	}

	TEST_CASE("mouse motion and wheel add up until the frame ends, positions are relative to the view") {
		InputState state;
		state.set_view(Vec2(100.F, 50.F), Vec2(640.F, 480.F));
		feed(state, MouseMovedEvent(110.F, 60.F));
		CHECK(state.get_mouse_motion() == Vec2(0.F));

		feed(state, MouseMovedEvent(115.F, 58.F));
		feed(state, MouseMovedEvent(120.F, 70.F));
		feed(state, MouseScrolledEvent(0.F, 1.F));
		feed(state, MouseScrolledEvent(0.F, 2.F));

		CHECK(state.get_mouse_position() == Vec2(20.F, 20.F));
		CHECK(state.get_mouse_motion() == Vec2(10.F, 10.F));
		CHECK(state.get_scroll() == Vec2(0.F, 3.F));
		CHECK(state.get_view_size() == Vec2(640.F, 480.F));

		state.end_frame();
		CHECK(state.get_mouse_motion() == Vec2(0.F));
		CHECK(state.get_scroll() == Vec2(0.F));
		CHECK(state.get_mouse_position() == Vec2(20.F, 20.F));
	}

	TEST_CASE("release_all lets go of everything, clear forgets it") {
		InputState state;
		feed(state, KeyPressedEvent(KeyCode::A, 0));
		feed(state, MouseButtonPressedEvent(MouseButton::Right));
		state.end_frame();

		state.release_all();
		CHECK_FALSE(state.is_key_down(KeyCode::A));
		CHECK(state.is_key_just_released(KeyCode::A));
		CHECK(state.is_mouse_button_just_released(MouseButton::Right));

		state.clear();
		CHECK_FALSE(state.is_key_just_released(KeyCode::A));
		CHECK_FALSE(state.is_mouse_button_just_released(MouseButton::Right));
	}
}
