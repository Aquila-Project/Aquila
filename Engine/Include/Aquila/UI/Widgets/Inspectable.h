#ifndef AQUILA_UI_WIDGETS_INSPECTABLE_H
#define AQUILA_UI_WIDGETS_INSPECTABLE_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Reflection {
class TypeInfo;
}

namespace Aquila::UI::Core {

class PropertyGrid;

struct InspectorAction {
	std::string text{};
	std::string icon{};
	std::string tooltip{};
	Delegate<void()> run;
	Delegate<bool()> enabled{};
};

/// Rows go into the section's own grid, so labels line up with the reflected ones.
class IInspectorContent {
  public:
	virtual ~IInspectorContent() = default;

	virtual void build_above(PropertyGrid &grid) {}
	virtual void build_below(PropertyGrid &grid) {}
	/// The data changed elsewhere, read it again without rebuilding.
	virtual void refresh() {}
	/// Runs before InspectorSection::on_edited.
	virtual void on_property_edited() {}

	/// Call it after changing the data yourself, the section re-reads and runs on_edited.
	Delegate<void()> notify_edited;
};

struct InspectorSection {
	/// Stable across selections, it keeps the collapsed state.
	std::string id;
	/// Empty: no header, can't collapse.
	std::string title;
	std::string icon;
	std::vector<InspectorAction> header_actions;

	const Reflection::TypeInfo *type = nullptr;
	/// Asked on every show and refresh. Null hides the section.
	Delegate<void *()> instance;
	Delegate<void()> on_edited;

	Unique<IInspectorContent> content;
};

struct InspectorTab {
	/// Also remembers the active tab between selections.
	std::string title;
	std::string icon{};
	bool always_visible = false;
	std::vector<InspectorSection> sections{};
	std::vector<InspectorAction> actions{};
};

/// Move only, the view owns it while shown.
struct Inspectable {
	std::string icon;
	std::string title;
	/// Set makes the title editable.
	Delegate<void(const std::string &)> rename;
	std::string subtitle;
	std::string subtitle_tooltip;
	std::vector<InspectorTab> tabs;
};

}

#endif
