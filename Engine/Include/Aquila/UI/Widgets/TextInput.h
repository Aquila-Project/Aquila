#pragma once

#include "Aquila/Foundation/Timer.h"
#include "Aquila/UI/Widgets/Control.h"
#include "Aquila/UI/Core/TextInputState.h"
#include "Aquila/UI/Text/FontAtlas.h"

namespace Aquila::UI::Core {

class TextInput : public Control {
  public:
	TextInput();
	explicit TextInput(std::string placeholder);

	[[nodiscard]] std::string_view get_type_name() const override { return "TextInput"; }
	static constexpr ViewKind k_kind = ViewKind::TextInput;
	[[nodiscard]] ViewKind get_kind() const override { return k_kind; }

	void set_text(const std::string &text);
	void set_font(Text::FontAtlas *font) override;
	void set_placeholder(std::string text);
	void ignore_next_char() { m_ignore_next_char = true; }
	Signal<void(const std::string &)> on_changed;
	Signal<void(const std::string &)> on_submit;

	[[nodiscard]] const std::string &get_text() const { return m_state.text; }
	[[nodiscard]] Vec2 get_intrinsic_size() const override;

	void on_mouse_press(Platform::MouseButton btn, Vec2 pos) override;
	void on_mouse_move(Vec2 pos) override;
	void on_key_press(Platform::KeyCode key, int mods = 0) override;
	void on_char_input(Uint32 codepoint) override;
	void on_focus_gained() override;
	void on_focus_lost() override;
	bool on_update(F32 delta_time) override;
	void on_draw_self(Rendering::DrawList &draw_list) override;

  protected:
	[[nodiscard]] Text::FontAtlas *resolve_font() const;
	// Adjusts m_ScrollOffsetX so the cursor stays within the visible text area.
	void clamp_scroll_offset(Text::FontAtlas *font, float scale, float visible_width);
	void reset_blink();

	TextInputState m_state;
	std::string m_placeholder;
	Text::FontAtlas *m_font = nullptr;
	float m_scroll_offset_x = 0.F; // horizontal scroll offset in pixels
	Foundation::TimePoint m_blink_start = Foundation::now();
	bool m_caret_visible = true;
	bool m_ignore_next_char = false;
};

} // namespace Aquila::UI::Core
