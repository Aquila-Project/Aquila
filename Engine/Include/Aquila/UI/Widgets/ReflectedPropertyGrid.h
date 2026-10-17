#ifndef AQUILA_UI_WIDGETS_REFLECTED_PROPERTY_GRID_H
#define AQUILA_UI_WIDGETS_REFLECTED_PROPERTY_GRID_H

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Reflection/TypeInfo.h"
#include "Aquila/UI/Core/View.h"

namespace Aquila::GFX {
class GfxContext;
}

namespace Aquila::UI::Core {

class PropertyGrid;

/// Fills a PropertyGrid from a TypeInfo: one widget per property, picked from its kind and hints.
/// Edits go through the property setters.
///
/// @code
/// ReflectedPropertyGrid fields(grid, fog_type, &ctx);
/// fields.bind(&fog, [this] { m_dirty = true; });
/// @endcode
class ReflectedPropertyGrid {
  public:
	/// `ctx` is only for color pickers. Without it colors show up as number fields.
	ReflectedPropertyGrid(PropertyGrid &grid, const Reflection::TypeInfo &type, GFX::GfxContext *ctx);
	~ReflectedPropertyGrid();

	AQUILA_NONCOPYABLE(ReflectedPropertyGrid);
	AQUILA_NONMOVEABLE(ReflectedPropertyGrid);

	/// `on_edited` runs after every edit the user makes.
	void bind(void *instance, Delegate<void()> on_edited = {});

	/// Reads the values into the widgets again, when the instance changed somewhere else.
	void refresh();

	[[nodiscard]] View *get_widget(std::string_view property_name) const;

  private:
	struct Row {
		const Reflection::Property *property = nullptr;
		View *widget = nullptr;
		Delegate<void(const Reflection::PropertyValue &)> show;
	};

	void add_row(PropertyGrid &grid, const Reflection::Property &property, GFX::GfxContext *ctx);
	void on_widget_edited(size_t row_index, const Reflection::PropertyValue &value);

	std::vector<Row> m_rows;
	void *m_instance = nullptr;
	Delegate<void()> m_on_edited;
	bool m_refreshing = false;
};

}

#endif
