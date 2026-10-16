#pragma once

#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/Label.h"

namespace Aquila::UI::Core {

class PropertyGrid : public View {
  public:
	explicit PropertyGrid(float label_width = 100.F);

	[[nodiscard]] std::string_view get_type_name() const override { return "PropertyGrid"; }

	void set_label_width(float w) { m_label_width = w; }
	[[nodiscard]] float get_label_width() const { return m_label_width; }

	enum class RowLayout : Uint8 { Inline, Stacked };

	View *add_row(std::string label, Unique<View> widget, RowLayout layout = RowLayout::Inline);

	template <typename T, typename... Args> T *add_row(std::string label, Args &&...args) {
		return static_cast<T *>(add_row(std::move(label), std::make_unique<T>(std::forward<Args>(args)...)));
	}

	template <typename T, typename... Args> T *add_stacked_row(std::string label, Args &&...args) {
		return static_cast<T *>(
			add_row(std::move(label), std::make_unique<T>(std::forward<Args>(args)...), RowLayout::Stacked));
	}

	View *add_check_row(std::string label, Unique<View> widget);

	template <typename T, typename... Args> T *add_check_row(std::string label, Args &&...args) {
		return static_cast<T *>(add_check_row(std::move(label), std::make_unique<T>(std::forward<Args>(args)...)));
	}

	void add_separator();

	void set_split(bool split);
	[[nodiscard]] bool is_split() const { return m_split; }

  private:
	View *add_split_row(std::string label, Unique<View> widget, RowLayout layout);

	float m_label_width;
	bool m_split = false;
};

} // namespace Aquila::UI::Core
