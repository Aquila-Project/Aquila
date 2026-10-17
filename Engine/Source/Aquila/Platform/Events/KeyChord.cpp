#include "Aquila/Platform/Events/KeyChord.h"

#include "Aquila/Foundation/Text/StringUtils.h"

#include <charconv>

namespace Aquila::Platform::Events {

namespace {

struct KeyName {
	KeyCode key;
	std::string_view name;
};

struct ModifierName {
	int mod;
	std::string_view name;
};

constexpr std::array k_key_names{
	KeyName{ KeyCode::Space, "Space" },
	KeyName{ KeyCode::Apostrophe, "'" },
	KeyName{ KeyCode::Comma, "," },
	KeyName{ KeyCode::Minus, "-" },
	KeyName{ KeyCode::Period, "." },
	KeyName{ KeyCode::Slash, "/" },
	KeyName{ KeyCode::Semicolon, ";" },
	KeyName{ KeyCode::Equal, "=" },
	KeyName{ KeyCode::LeftBracket, "[" },
	KeyName{ KeyCode::Backslash, "\\" },
	KeyName{ KeyCode::RightBracket, "]" },
	KeyName{ KeyCode::GraveAccent, "`" },
	KeyName{ KeyCode::Escape, "Esc" },
	KeyName{ KeyCode::Enter, "Enter" },
	KeyName{ KeyCode::Tab, "Tab" },
	KeyName{ KeyCode::Backspace, "Backspace" },
	KeyName{ KeyCode::Insert, "Insert" },
	KeyName{ KeyCode::Delete, "Delete" },
	KeyName{ KeyCode::Right, "Right" },
	KeyName{ KeyCode::Left, "Left" },
	KeyName{ KeyCode::Down, "Down" },
	KeyName{ KeyCode::Up, "Up" },
	KeyName{ KeyCode::PageUp, "PageUp" },
	KeyName{ KeyCode::PageDown, "PageDown" },
	KeyName{ KeyCode::Home, "Home" },
	KeyName{ KeyCode::End, "End" },
	KeyName{ KeyCode::KeypadAdd, "Keypad+" },
	KeyName{ KeyCode::KeypadSubtract, "Keypad-" },
	KeyName{ KeyCode::KeypadEnter, "KeypadEnter" },
};

constexpr std::array k_key_aliases{
	KeyName{ KeyCode::Escape, "Escape" }, KeyName{ KeyCode::Enter, "Return" },	KeyName{ KeyCode::Delete, "Del" },
	KeyName{ KeyCode::Insert, "Ins" },	  KeyName{ KeyCode::Equal, "Equal" },	KeyName{ KeyCode::Minus, "Minus" },
	KeyName{ KeyCode::Comma, "Comma" },	  KeyName{ KeyCode::Period, "Period" }, KeyName{ KeyCode::PageUp, "PgUp" },
	KeyName{ KeyCode::PageDown, "PgDn" },
};

constexpr std::array k_modifier_names{
	ModifierName{ MODIFIER_CONTROL, "ctrl" }, ModifierName{ MODIFIER_CONTROL, "control" },
	ModifierName{ MODIFIER_SHIFT, "shift" },  ModifierName{ MODIFIER_ALT, "alt" },
	ModifierName{ MODIFIER_ALT, "option" },	  ModifierName{ MODIFIER_SUPER, "super" },
	ModifierName{ MODIFIER_SUPER, "cmd" },	  ModifierName{ MODIFIER_SUPER, "meta" },
};

constexpr std::string_view k_letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr std::string_view k_digits = "0123456789";
constexpr std::array<std::string_view, 12> k_function_keys{ "F1", "F2", "F3", "F4",	 "F5",	"F6",
															"F7", "F8", "F9", "F10", "F11", "F12" };

constexpr int code_of(KeyCode key) {
	return static_cast<int>(key);
}

std::string_view trim(std::string_view text) {
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
		text.remove_prefix(1);
	}
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
		text.remove_suffix(1);
	}
	return text;
}

bool equals_ignoring_case(std::string_view a, std::string_view b) {
	return a.size() == b.size() && Foundation::to_lower(a) == Foundation::to_lower(b);
}

int modifier_from_name(std::string_view name) {
	for (const ModifierName &modifier : k_modifier_names) {
		if (equals_ignoring_case(name, modifier.name)) {
			return modifier.mod;
		}
	}
	return 0;
}

Option<int> parse_number(std::string_view text) {
	int value = 0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (error != std::errc{} || end != text.data() + text.size()) {
		return std::nullopt;
	}
	return value;
}

}

std::string_view key_name(KeyCode key) {
	const int code = code_of(key);
	if (code >= code_of(KeyCode::A) && code <= code_of(KeyCode::Z)) {
		return k_letters.substr(static_cast<Usize>(code - code_of(KeyCode::A)), 1);
	}
	if (code >= code_of(KeyCode::Num0) && code <= code_of(KeyCode::Num9)) {
		return k_digits.substr(static_cast<Usize>(code - code_of(KeyCode::Num0)), 1);
	}
	if (code >= code_of(KeyCode::F1) && code <= code_of(KeyCode::F12)) {
		return k_function_keys[static_cast<Usize>(code - code_of(KeyCode::F1))];
	}
	for (const KeyName &entry : k_key_names) {
		if (entry.key == key) {
			return entry.name;
		}
	}
	return {};
}

Option<KeyCode> key_from_name(std::string_view name) {
	name = trim(name);
	if (name.empty()) {
		return std::nullopt;
	}

	if (name.size() == 1) {
		const auto c = static_cast<unsigned char>(name.front());
		if (std::isalpha(c) != 0) {
			return static_cast<KeyCode>(code_of(KeyCode::A) + (std::toupper(c) - 'A'));
		}
		if (std::isdigit(c) != 0) {
			return static_cast<KeyCode>(code_of(KeyCode::Num0) + (c - '0'));
		}
	}

	if (name.size() > 1 && (name.front() == 'F' || name.front() == 'f')) {
		if (const Option<int> number = parse_number(name.substr(1)); number && *number >= 1 && *number <= 12) {
			return static_cast<KeyCode>(code_of(KeyCode::F1) + *number - 1);
		}
	}

	if (name.size() > 3 && equals_ignoring_case(name.substr(0, 3), "key")) {
		if (const Option<int> code = parse_number(name.substr(3)); code && *code > 0 && *code < 512) {
			return static_cast<KeyCode>(*code);
		}
	}

	for (const auto &table : { std::span<const KeyName>(k_key_names), std::span<const KeyName>(k_key_aliases) }) {
		for (const KeyName &entry : table) {
			if (equals_ignoring_case(name, entry.name)) {
				return entry.key;
			}
		}
	}
	return std::nullopt;
}

Option<KeyChord> KeyChord::parse(std::string_view text) {
	text = trim(text);
	int mods = 0;
	while (true) {
		const Usize plus = text.find('+');
		if (plus == std::string_view::npos || plus == 0) {
			break;
		}
		const int mod = modifier_from_name(trim(text.substr(0, plus)));
		if (mod == 0) {
			break;
		}
		mods |= mod;
		text = text.substr(plus + 1);
	}

	const Option<KeyCode> key = key_from_name(text);
	if (!key) {
		return std::nullopt;
	}
	return KeyChord{ .key = *key, .mods = mods };
}

KeyChord KeyChord::from(const KeyPressedEvent &event) {
	return { .key = event.get_key_code(), .mods = event.get_mods() & MODIFIER_MASK };
}

std::string KeyChord::to_string() const {
	std::string out;
	if ((mods & MODIFIER_CONTROL) != 0) {
		out += "Ctrl+";
	}
	if ((mods & MODIFIER_SHIFT) != 0) {
		out += "Shift+";
	}
	if ((mods & MODIFIER_ALT) != 0) {
		out += "Alt+";
	}
	if ((mods & MODIFIER_SUPER) != 0) {
		out += "Super+";
	}
	const std::string_view name = key_name(key);
	out += name.empty() ? "Key" + std::to_string(code_of(key)) : std::string(name);
	return out;
}

bool KeyChord::is_modifier_only() const {
	const int code = code_of(key);
	return code >= code_of(KeyCode::LeftShift) && code <= code_of(KeyCode::RightAlt) + 1;
}

bool KeyChord::works_while_typing() const {
	const int code = code_of(key);
	if (code >= code_of(KeyCode::F1) && code <= code_of(KeyCode::F12)) {
		return true;
	}
	if ((mods & (MODIFIER_CONTROL | MODIFIER_ALT | MODIFIER_SUPER)) == 0) {
		return false;
	}
	if ((mods & (MODIFIER_ALT | MODIFIER_SUPER)) != 0) {
		return true;
	}
	switch (key) {
	case KeyCode::A:
	case KeyCode::C:
	case KeyCode::V:
	case KeyCode::X:
	case KeyCode::Y:
	case KeyCode::Z:
	case KeyCode::Left:
	case KeyCode::Right:
	case KeyCode::Home:
	case KeyCode::End:
	case KeyCode::Backspace:
	case KeyCode::Delete:
		return false;
	default:
		return true;
	}
}

}
