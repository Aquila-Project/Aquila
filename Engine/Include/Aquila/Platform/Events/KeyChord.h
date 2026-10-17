#ifndef AQUILA_PLATFORM_EVENTS_KEY_CHORD_H
#define AQUILA_PLATFORM_EVENTS_KEY_CHORD_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Platform/Events/InputEvent.h"

namespace Aquila::Platform::Events {

static constexpr int MODIFIER_MASK = MODIFIER_SHIFT | MODIFIER_CONTROL | MODIFIER_ALT | MODIFIER_SUPER;

/// A key plus modifiers, used for shortcuts.
///
/// @code
/// Option<KeyChord> save = KeyChord::parse("Ctrl+Shift+S");
/// save->to_string();
/// KeyChord::from(key_event) == *save;
/// @endcode
struct KeyChord {
	KeyCode key = KeyCode::Unknown;
	int mods = 0;

	/// Case doesn't matter. Modifiers: Ctrl/Control, Shift, Alt/Option, Super/Cmd/Meta.
	/// Keys without a name can be written as Key<code>, like "Key161". nullopt for an unknown key.
	[[nodiscard]] static Option<KeyChord> parse(std::string_view text);
	/// Caps Lock and Num Lock are dropped, so they don't break shortcuts.
	[[nodiscard]] static KeyChord from(const KeyPressedEvent &event);

	/// The format parse() reads, modifiers always in Ctrl, Shift, Alt, Super order.
	[[nodiscard]] std::string to_string() const;
	[[nodiscard]] bool is_valid() const { return key != KeyCode::Unknown; }
	[[nodiscard]] bool is_modifier_only() const;
	/// Whether the shortcut still fires while a text field has focus: F-keys and Ctrl/Alt/Super chords,
	/// except the ones text fields use themselves (Ctrl+C, Ctrl+Z, Ctrl+Left...).
	[[nodiscard]] bool works_while_typing() const;

	bool operator==(const KeyChord &) const = default;
};

/// Empty for keys without a name.
[[nodiscard]] std::string_view key_name(KeyCode key);
[[nodiscard]] Option<KeyCode> key_from_name(std::string_view name);

}

#endif
