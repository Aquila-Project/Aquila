#ifndef AQUILA_UI_WIDGETS_SLIDER_FIELD_H
#define AQUILA_UI_WIDGETS_SLIDER_FIELD_H

#include "Aquila/UI/Widgets/Control.h"

namespace Aquila::UI::Core {

class DragFloat;
class Label;
class SliderFieldTrack;

/// A slider with the exact value next to it, Unity style: drag the track to get close, click the number to type it.
/// The track can be logarithmic (every doubling the same distance), carry labelled marks (dragging snaps to them and
/// the nearest one is named under the track) and a color gradient. on_changed only fires for user edits.
class SliderField : public Control {
  public:
	struct Mark {
		F32 value = 0.F;
		std::string label;
	};

	SliderField();

	[[nodiscard]] std::string_view get_type_name() const override { return "SliderField"; }

	/// Doesn't fire on_changed.
	void set_value(F32 value);
	[[nodiscard]] F32 get_value() const { return m_value; }

	/// What typing is clamped to.
	void set_range(F32 min, F32 max);
	/// What the track covers, can be narrower than set_range. A logarithmic track needs min > 0.
	void set_slider_range(F32 min, F32 max);
	void set_logarithmic(bool logarithmic);
	/// The most decimals shown, big values show fewer.
	void set_precision(int decimals);
	void set_suffix(std::string suffix);
	void set_marks(std::vector<Mark> marks);
	/// Colors spread evenly along the track, drawn instead of the fill.
	void set_gradient(std::vector<Vec4> colors);

	/// Where a value sits along the track, 0..1, and back.
	[[nodiscard]] F32 to_track(F32 value) const;
	[[nodiscard]] F32 from_track(F32 t) const;
	[[nodiscard]] const std::vector<Mark> &get_marks() const { return m_marks; }
	[[nodiscard]] const std::vector<Vec4> &get_gradient() const { return m_gradient; }

	Signal<void(float)> on_changed;

  private:
	friend class SliderFieldTrack;

	void edit_from_track(F32 t);
	void refresh();

	SliderFieldTrack *m_track = nullptr;
	DragFloat *m_box = nullptr;
	Label *m_caption = nullptr;

	F32 m_value = 0.F;
	F32 m_min = -1e18F;
	F32 m_max = 1e18F;
	F32 m_slider_min = 0.F;
	F32 m_slider_max = 1.F;
	int m_precision = 2;
	bool m_logarithmic = false;
	std::vector<Mark> m_marks;
	std::vector<Vec4> m_gradient;
};

}

#endif
