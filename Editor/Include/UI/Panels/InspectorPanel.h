#pragma once

#include "Aquila/Scene/Entity.h"
#include "UI/Inspectors/IComponentUI.h"
#include "UI/Panels/IEditorPanel.h"

namespace Aquila::GFX {
class GfxContext;
class GfxTexture;
} // namespace Aquila::GFX
namespace Aquila::Graphics {
class Material;
} // namespace Aquila::Graphics
namespace Aquila::UI::Core {
class Button;
class Collapsible;
class Image;
class Label;
class PopupMenu;
class ScrollView;
class TextInput;
class TextureCache;
class View;
} // namespace Aquila::UI::Core

namespace Editor {

class InspectorPanel : public IEditorPanel {
  public:
	InspectorPanel(Aquila::GFX::GfxContext &context, Aquila::UI::Core::TextureCache *texture_cache);
	void build(Aquila::UI::Core::View *panel, Aquila::UI::Core::View *overlay_root) override;
	Signal<void(Aquila::SceneManagement::Entity)> on_entity_renamed;
	Signal<void(Aquila::SceneManagement::Entity)> on_components_changed;
	void show_entity(Aquila::SceneManagement::Entity entity);
	/// Reads the values again without rebuilding the sections, cheap enough while the gizmo drags.
	void refresh_values(Aquila::SceneManagement::Entity entity);
	void clear();
	void open_add_search(Vec2 canvas_pos);

  private:
	struct Tab {
		bool always_visible = false;
		Aquila::UI::Core::Button *button = nullptr;
		Aquila::UI::Core::View *page = nullptr;
		Aquila::UI::Core::View *sections = nullptr;
	};

	struct Section {
		Usize tab = 0;
		Aquila::UI::Core::Collapsible *collapsible = nullptr;
		Unique<IComponentUI> ui;
	};

	struct AddableComponent {
		std::string name;
		std::string category;
		Delegate<bool(Aquila::SceneManagement::Entity)> present;
		Delegate<void(Aquila::SceneManagement::Entity)> attach;
	};

	struct AddMenuGroup {
		std::string name;
		Aquila::GFX::GfxTexture *icon = nullptr;
	};

	[[nodiscard]] Aquila::GFX::GfxTexture *icon(std::string_view name) const;
	Usize add_tab(const std::string &title, std::string_view icon_name, bool always_visible);
	void add_section(Usize tab, const std::string &component, Unique<IComponentUI> ui);
	void build_tabs_and_sections();
	void build_add_button(Aquila::UI::Core::View &page);
	void select_tab(Usize tab);
	[[nodiscard]] bool tab_has_content(Usize tab) const;
	void refresh_tabs();
	void style_dropdowns(Aquila::UI::Core::View &root) const;

	void build_component_registry();
	Ref<Aquila::Graphics::Material> ensure_default_material();
	void attach_default_material(Aquila::SceneManagement::Entity entity);
	void open_add_menu();
	void open_add_popup_at(Vec2 canvas_pos, bool swallow_first_char);
	void populate_add_menu(const std::string &query);

	Aquila::GFX::GfxContext &m_context;
	Ref<Aquila::Graphics::Material> m_default_material;
	Aquila::UI::Core::TextureCache *m_texture_cache = nullptr;
	Aquila::UI::Core::View *m_tab_strip = nullptr;
	Aquila::UI::Core::View *m_pages = nullptr;
	Aquila::UI::Core::View *m_head = nullptr;
	Aquila::UI::Core::Image *m_head_icon = nullptr;
	Aquila::UI::Core::ScrollView *m_scroll_view = nullptr;
	Aquila::UI::Core::View *m_empty_state = nullptr;
	Aquila::UI::Core::TextInput *m_name_input = nullptr;
	Aquila::UI::Core::Label *m_uuid = nullptr;
	Aquila::UI::Core::Button *m_add_button = nullptr;
	Aquila::UI::Core::PopupMenu *m_add_popup = nullptr;
	Aquila::UI::Core::TextInput *m_add_search = nullptr;
	std::vector<AddableComponent> m_addable;
	std::vector<AddMenuGroup> m_categories;

	std::vector<Tab> m_tabs;
	std::vector<Section> m_sections;
	Usize m_active_tab = 0;

	Aquila::SceneManagement::Entity m_current_entity;
	bool m_has_current = false;
};

} // namespace Editor
