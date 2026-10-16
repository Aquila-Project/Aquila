#pragma once

#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/Button.h"

namespace Aquila::UI::Core {

class TabView : public View {
  public:
	TabView();

	[[nodiscard]] std::string_view get_type_name() const override { return "TabView"; }

	View *add_tab(std::string title, GFX::GfxTexture *icon = nullptr);

	void set_active_tab(int index);
	[[nodiscard]] int get_active_tab() const { return m_active_tab; }
	Signal<void(int)> on_tab_changed;

  private:
	void apply_active_tab();

	struct Tab {
		Button *button = nullptr;
		View *panel = nullptr;
	};

	View *m_tab_bar = nullptr;
	View *m_panels = nullptr;
	std::vector<Tab> m_tabs;
	int m_active_tab = -1;
};

} // namespace Aquila::UI::Core
