#pragma once

#include "Aquila/Platform/Input.h"
#include "Aquila/UI/Text/FontAtlas.h"

namespace Aquila::UI::Core {

/// The text and selection behind an editable field. cursor and select_anchor are byte offsets into UTF-8 text.
struct TextInputState {
	std::string text;
	size_t cursor = 0;
	size_t select_anchor = 0;

	[[nodiscard]] bool has_selection() const { return cursor != select_anchor; }
	[[nodiscard]] size_t selection_min() const { return std::min(cursor, select_anchor); }
	[[nodiscard]] size_t selection_max() const { return std::max(cursor, select_anchor); }

	void set_text(std::string t) {
		text = std::move(t);
		cursor = text.size();
		select_anchor = cursor;
	}

	void select_all() {
		select_anchor = 0;
		cursor = text.size();
	}

	void delete_selection() {
		const size_t lo = selection_min();
		const size_t hi = selection_max();
		text.erase(lo, hi - lo);
		cursor = lo;
		select_anchor = lo;
	}

	/// @return true when it used the key
	bool handle_key_press(Platform::KeyCode key, int mods);
	bool handle_char_input(Uint32 codepoint);

	[[nodiscard]] float measure_to_pos(const Text::FontAtlas &font, float scale, size_t pos) const;

	[[nodiscard]] size_t hit_test_pos(const Text::FontAtlas &font, float scale, float local_x) const;
};

} // namespace Aquila::UI::Core
