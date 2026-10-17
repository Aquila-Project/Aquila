#pragma once

#include "Aquila/UI/Widgets/Control.h"
#include "Aquila/UI/Core/TextInputState.h"
#include "Aquila/UI/Text/FontAtlas.h"

namespace Aquila::UI::Core {

/// A number you drag sideways, or click to type in. on_changed only fires for user edits.
class DragFloat : public Control {
  public:
	struct Config {
		float min = -1e18F;
		float max = 1e18F;
		float step = 0.F;
		float speed = 1.F;
		int precision = 3;
		std::string prefix{};
	};

	DragFloat();
	explicit DragFloat(const Config &config);

	[[nodiscard]] std::string_view get_type_name() const override { return "DragFloat"; }

	/// Clamps and snaps to the step. Doesn't fire on_changed.
	void set_value(float value);
	void set_range(float min, float max);
	void set_step(float step);
	/// Value change per pixel dragged, despite the parameter name.
	void set_speed(float pixels_per_unit);
	void set_precision(int decimals);
	void set_prefix(std::string prefix);
	/// Drawn inside the field on the left, like X/Y/Z in stacked vectors.
	void set_label(std::string label);
	void set_suffix(std::string suffix);
	/// Draws a fill bar between min and max.
	void set_slider(bool slider);
	Signal<void(float)> on_changed;

	[[nodiscard]] float get_value() const { return m_value; }

	void on_mouse_press(Platform::MouseButton btn, Vec2 pos) override;
	void on_mouse_release(Platform::MouseButton btn, Vec2 pos) override;
	void on_mouse_move(Vec2 pos) override;
	void on_key_press(Platform::KeyCode key, int mods = 0) override;
	void on_char_input(Uint32 codepoint) override;
	void on_focus_lost() override;
	void on_draw_self(Rendering::DrawList &draw_list) override;

  protected:
	[[nodiscard]] virtual std::string format_value() const;

	virtual void on_value_committed() {}

	float m_value = 0.F;
	float m_min = -1e18F;
	float m_max = 1e18F;
	float m_step = 0.F;
	float m_speed = 1.0F;
	int m_precision = 3;
	std::string m_prefix;
	std::string m_label;
	std::string m_suffix;
	bool m_slider = false;

  private:
	enum class Mode { Drag, Edit };

	void enter_edit_mode();
	void commit_edit();
	void cancel_edit();
	[[nodiscard]] Text::FontAtlas *resolve_font() const;

	Mode m_mode = Mode::Drag;
	TextInputState m_edit_state;
	float m_drag_start_value = 0.F;
	float m_drag_start_x = 0.F;
	bool m_has_dragged = false;

	static constexpr float K_DRAG_THRESHOLD = 3.F;
};

} // namespace Aquila::UI::Core
