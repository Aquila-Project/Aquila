#include <doctest.h>

#include "Aquila/Platform/Events/KeyChord.h"

using namespace Aquila::Platform::Events;

namespace {

KeyChord chord(std::string_view text) {
	const auto parsed = KeyChord::parse(text);
	REQUIRE_MESSAGE(parsed.has_value(), text);
	return *parsed;
}

}

TEST_SUITE("KeyChord") {
	TEST_CASE("chords round trip through text") {
		for (const char *text : { "Ctrl+Shift+S", "F5", "Delete", "Ctrl+,", "Keypad+", "Ctrl+Keypad-", "Shift+Tab",
								  "Ctrl+PageDown", "Alt+Super+1", "Esc", "Ctrl+=", "W" }) {
			CHECK(chord(text).to_string() == text);
		}
	}

	TEST_CASE("parsing ignores case, spaces and modifier order") {
		CHECK(chord("ctrl + shift + s") == KeyChord{ .key = KeyCode::S, .mods = MODIFIER_CONTROL | MODIFIER_SHIFT });
		CHECK(chord("Shift+Control+S") == chord("Ctrl+Shift+S"));
		CHECK(chord("Escape") == chord("Esc"));
		CHECK(chord("cmd+del") == KeyChord{ .key = KeyCode::Delete, .mods = MODIFIER_SUPER });
		CHECK(chord("f12").key == KeyCode::F12);
	}

	TEST_CASE("invalid chords are rejected") {
		for (const char *text : { "", "Ctrl+", "Ctrl+Foo", "Hyper+A", "F13", "Ctrl++" }) {
			CHECK_FALSE_MESSAGE(KeyChord::parse(text).has_value(), text);
		}
	}

	TEST_CASE("keys without a name use their code") {
		const KeyChord keypad_one{ .key = static_cast<KeyCode>(321), .mods = MODIFIER_CONTROL };
		CHECK(keypad_one.to_string() == "Ctrl+Key321");
		CHECK(chord("Ctrl+Key321") == keypad_one);
	}

	TEST_CASE("events keep only real modifiers") {
		const KeyPressedEvent event(KeyCode::S, 0, MODIFIER_CONTROL | 0x0010 | 0x0020);
		CHECK(KeyChord::from(event) == chord("Ctrl+S"));
	}

	TEST_CASE("typing keeps plain keys and text editing chords") {
		CHECK_FALSE(chord("W").works_while_typing());
		CHECK_FALSE(chord("Shift+S").works_while_typing());
		CHECK_FALSE(chord("Delete").works_while_typing());
		CHECK_FALSE(chord("Ctrl+C").works_while_typing());
		CHECK_FALSE(chord("Ctrl+Shift+Left").works_while_typing());
		CHECK(chord("Ctrl+N").works_while_typing());
		CHECK(chord("Ctrl+Alt+C").works_while_typing());
		CHECK(chord("F5").works_while_typing());
	}

	TEST_CASE("modifier keys alone are not chords") {
		CHECK(KeyChord{ .key = KeyCode::LeftControl, .mods = MODIFIER_CONTROL }.is_modifier_only());
		CHECK(KeyChord{ .key = static_cast<KeyCode>(347) }.is_modifier_only());
		CHECK_FALSE(chord("A").is_modifier_only());
	}
}
