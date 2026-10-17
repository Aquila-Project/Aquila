#pragma once

#include "Aquila/UI/Widgets/FloatingOverlay.h"
#include "Aquila/UI/Widgets/Button.h"

namespace Aquila::GFX {
class GfxTexture;
}

namespace Aquila::UI::Core {

class TextInput;

class PopupMenu : public FloatingOverlay {
  public:
	PopupMenu();

	[[nodiscard]] std::string_view get_type_name() const override { return "PopupMenu"; }

	void add_item(std::string text, Delegate<void()> callback);
	void add_item(std::string text, std::string shortcut, GFX::GfxTexture *icon, Delegate<void()> callback);
	void add_separator();
	PopupMenu *add_submenu(std::string text, GFX::GfxTexture *icon = nullptr);

	/// A search field on top that filters the items as you type.
	TextInput *enable_search(std::string placeholder);

	void set_submenu_icon(GFX::GfxTexture *texture) { m_submenu_icon = texture; }
	/// Runs after any item ran and the menu closed.
	void set_on_activate(Delegate<void()> callback) { m_on_activate = std::move(callback); }

	void clear_items();
	void refresh();
	void open_at(Vec2 canvas_pos);
	void open_below(Vec2 anchor_pos, float anchor_height);
	void dismiss();

  private:
	struct Item {
		std::string text;
		std::string shortcut;
		GFX::GfxTexture *icon = nullptr;
		Delegate<void()> callback;
		PopupMenu *submenu = nullptr;
		bool is_separator = false;
	};

	void rebuild();
	PopupMenu *root_menu();
	void open_submenu(PopupMenu *submenu, Vec2 at);
	void close_submenu();

	std::vector<Item> m_items;
	std::vector<View *> m_item_views;
	PopupMenu *m_parent_menu = nullptr;
	PopupMenu *m_open_submenu = nullptr;
	GFX::GfxTexture *m_submenu_icon = nullptr;
	TextInput *m_search_input = nullptr;
	Delegate<void()> m_on_activate;
};

} // namespace Aquila::UI::Core
