#ifndef AQUILA_UI_WIDGETS_TAB_BAR_H
#define AQUILA_UI_WIDGETS_TAB_BAR_H

#include "Aquila/Foundation/Signal.h"
#include "Aquila/UI/Core/View.h"

namespace Aquila::GFX {
class GfxTexture;
}

namespace Aquila::UI::Core {

class Button;

/// Tabs get the tab class (`tab-class` in markup), the active one also "<tab class>-active".
/// A click fires on_selected even on the active tab, set_active() never fires.
class TabBar : public View {
  public:
	TabBar();
	explicit TabBar(std::string tab_class);

	[[nodiscard]] std::string_view get_type_name() const override { return "TabBar"; }

	Button *add_tab(std::string label, GFX::GfxTexture *icon = nullptr);
	void set_active(int index);
	[[nodiscard]] int get_active() const { return m_active; }
	[[nodiscard]] Button *get_tab(int index) const;
	[[nodiscard]] int get_tab_count() const { return static_cast<int>(m_tabs.size()); }
	void set_tab_class(std::string tab_class);

	void apply_xml_attribute(std::string_view name, std::string_view value,
							 IResourceResolver *resolver = nullptr) override;
	bool add_markup_item(MarkupItem &item, IResourceResolver *resolver) override;

	Signal<void(int)> on_selected;

  private:
	void apply_classes();

	std::string m_tab_class;
	std::vector<Button *> m_tabs;
	int m_active = -1;
};

}

#endif
