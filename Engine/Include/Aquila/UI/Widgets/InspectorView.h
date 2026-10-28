#ifndef AQUILA_UI_WIDGETS_INSPECTOR_VIEW_H
#define AQUILA_UI_WIDGETS_INSPECTOR_VIEW_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/Signal.h"
#include "Aquila/UI/Widgets/Inspectable.h"

namespace Aquila::GFX {
class GfxContext;
class GfxTexture;
} // namespace Aquila::GFX

namespace Aquila::UI::Core {

class Button;
class Image;
class Label;
class LayoutLoader;
class ReflectedPropertyGrid;
class ScrollView;
class TextInput;
class View;

/// Needs the ids of inspector.aqlayout under `root`.
class InspectorView {
  public:
	using IconResolver = Delegate<GFX::GfxTexture *(std::string_view name)>;

	/// `ctx` is only for color pickers.
	InspectorView(View &root, IconResolver icons, GFX::GfxContext *ctx);
	~InspectorView();

	AQUILA_NONCOPYABLE(InspectorView);
	AQUILA_NONMOVEABLE(InspectorView);

	/// Safe from the shown inspectable's own callbacks, the old widgets live until update().
	void show(Inspectable inspectable);
	void clear();
	/// Cheap, fine while the gizmo drags.
	void refresh();
	/// Every frame.
	void update();

	[[nodiscard]] bool is_showing() const { return m_inspectable != nullptr; }
	[[nodiscard]] View *find_section(std::string_view id) const;
	[[nodiscard]] const std::string &get_active_tab() const { return m_shown_tab; }
	/// Later inspectables open on this tab when they have it.
	void select_tab(std::string_view title);

  private:
	struct ShownSection {
		InspectorSection *section = nullptr;
		Usize tab = 0;
		View *root = nullptr;
		Unique<ReflectedPropertyGrid> properties;
		bool visible = true;
	};

	struct ShownTab {
		InspectorTab *tab = nullptr;
		Button *button = nullptr;
		View *page = nullptr;
	};

	struct Retired {
		Unique<Inspectable> inspectable;
		std::vector<Unique<ReflectedPropertyGrid>> properties;
		std::vector<Unique<View>> views;
	};

	[[nodiscard]] bool has_layout() const;
	void retire();
	void build_header();
	void build_tab(LayoutLoader &loader, InspectorTab &tab);
	void build_section(LayoutLoader &loader, View &parent, InspectorSection &section, Usize tab);
	void build_actions(LayoutLoader &loader, View &parent, const std::vector<InspectorAction> &actions);
	void bind(ShownSection &shown);
	[[nodiscard]] bool is_usable(Usize tab) const;
	void refresh_tabs();
	void show_tab(Usize tab);
	void style_dropdowns(View &root) const;
	[[nodiscard]] GFX::GfxTexture *icon(std::string_view name) const;

	IconResolver m_icons;
	GFX::GfxContext *m_ctx = nullptr;

	View *m_tab_strip = nullptr;
	View *m_pages = nullptr;
	View *m_head = nullptr;
	Image *m_head_icon = nullptr;
	TextInput *m_title = nullptr;
	Label *m_subtitle = nullptr;
	View *m_empty = nullptr;
	ScrollView *m_scroll = nullptr;
	Signal<void(const std::string &)>::Connection m_title_connection;

	Unique<Inspectable> m_inspectable;
	std::vector<ShownTab> m_tabs;
	std::vector<ShownSection> m_sections;
	std::string m_preferred_tab;
	std::string m_shown_tab;
	std::unordered_map<std::string, bool> m_collapsed;
	std::vector<Retired> m_retired;
};

}

#endif
