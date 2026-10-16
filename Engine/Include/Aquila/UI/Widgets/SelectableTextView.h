#ifndef AQUILA_UI_WIDGETS_SELECTABLE_TEXT_VIEW_H
#define AQUILA_UI_WIDGETS_SELECTABLE_TEXT_VIEW_H

#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Text/TextSelection.h"

namespace Aquila::UI::Core {

class SelectableTextView : public View {
  public:
	SelectableTextView();

	[[nodiscard]] std::string_view get_type_name() const override { return "SelectableTextView"; }

	void add_line(std::string text, Vec4 color = Vec4(0.F), Int32 tag_begin = 0, Int32 tag_length = 0,
				  Vec4 tag_color = Vec4(0.F));
	void clear();
	void remove_front(Int32 count);

	void select_all();
	void clear_selection();
	[[nodiscard]] std::string get_selected_text() const;
	[[nodiscard]] Int32 get_line_count() const { return static_cast<Int32>(m_lines.size()); }

	Vec2 get_intrinsic_size() const override;

	void on_mouse_press(Platform::MouseButton btn, Vec2 pos) override;
	void on_mouse_move(Vec2 pos) override;
	void on_mouse_release(Platform::MouseButton btn, Vec2 pos) override;
	void on_key_press(Platform::KeyCode key, int mods = 0) override;
	void on_draw_self(Rendering::DrawList &draw_list) override;

  private:
	struct Row {
		Int32 line = 0;
		Int32 begin = 0;
		Int32 end = 0;
	};

	[[nodiscard]] F32 line_height() const;
	[[nodiscard]] const std::vector<Row> &rows() const;
	void wrap_line(Usize index, F32 width, Text::FontAtlas *font, F32 size) const;
	[[nodiscard]] Text::TextPosition position_at(Vec2 canvas_pos) const;
	[[nodiscard]] F32 width_of(std::string_view text) const;
	[[nodiscard]] Rect visible_rect() const;

	std::vector<std::string> m_lines;
	std::vector<Vec4> m_colors;
	struct Tag {
		Int32 begin = 0;
		Int32 length = 0;
		Vec4 color = Vec4(0.F);
	};
	std::vector<Tag> m_tags;
	Text::TextSelection m_selection;
	bool m_selecting = false;

	mutable std::vector<Row> m_rows;
	mutable Usize m_wrapped_lines = 0;
	mutable F32 m_rows_width = -1.F;
	mutable F32 m_rows_size = -1.F;
	mutable Text::FontAtlas *m_rows_font = nullptr;
};

} // namespace Aquila::UI::Core

#endif
