#pragma once

#include "Aquila/UI/Widgets/Control.h"

namespace Aquila::UI::Core {

/// A widget that holds one value. on_changed only fires when the value really changes.
///
/// @code
/// auto *grid_toggle = row->add_child<Checkbox>();
/// grid_toggle->set_value_without_notify(settings.show_grid);
/// grid_toggle->on_changed.connect([&settings](const bool &on) { settings.show_grid = on; });
/// @endcode
template <typename T> class BaseField : public Control {
  public:
	[[nodiscard]] const T &get_value() const { return m_value; }

	void set_value(const T &value) {
		const T coerced = coerce(value);
		if (coerced == m_value) {
			return;
		}
		m_value = coerced;
		on_value_updated();
		on_changed(m_value);
	}

	/// For syncing the widget from your data, so the change doesn't echo back through on_changed.
	void set_value_without_notify(const T &value) {
		m_value = coerce(value);
		on_value_updated();
	}

	Signal<void(const T &)> on_changed;

  protected:
	virtual void on_value_updated() {}

	/// Clamp or snap incoming values here.
	virtual T coerce(const T &value) const { return value; }

	T m_value{};
};

} // namespace Aquila::UI::Core
