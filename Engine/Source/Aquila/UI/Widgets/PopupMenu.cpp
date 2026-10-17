#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Aquila/UI/Widgets/Separator.h"
#include "Aquila/UI/Widgets/TextInput.h"

namespace Aquila::UI::Core {

PopupMenu::PopupMenu() : FloatingOverlay(10) {
	add_class("menu-dropdown");
	set_dismiss_on_click_away(true);

	FloatingConfig fc;
	fc.attach_to = FloatingAttachTo::Root;
	fc.element_point = FloatingAttachPoint::LeftTop;
	fc.parent_point = FloatingAttachPoint::LeftTop;
	fc.z_index = 50;
	set_floating(fc);
}

void PopupMenu::add_item(std::string text, Delegate<void()> callback) {
	m_items.push_back({ std::move(text), {}, nullptr, std::move(callback), nullptr, false });
}

void PopupMenu::add_item(std::string text, std::string shortcut, GFX::GfxTexture *icon, Delegate<void()> callback) {
	m_items.push_back({ std::move(text), std::move(shortcut), icon, std::move(callback), nullptr, false });
}

void PopupMenu::add_separator() {
	m_items.push_back({ {}, {}, nullptr, {}, nullptr, true });
}

TextInput *PopupMenu::enable_search(std::string placeholder) {
	if (m_search_input != nullptr) {
		return m_search_input;
	}
	auto input = std::make_unique<TextInput>(std::move(placeholder));
	input->add_class("menu-search");
	m_search_input = dynamic_cast<TextInput *>(add_child(std::move(input)));
	return m_search_input;
}

PopupMenu *PopupMenu::add_submenu(std::string text, GFX::GfxTexture *icon) {
	auto submenu_uniq = std::make_unique<PopupMenu>();
	auto *submenu = dynamic_cast<PopupMenu *>(add_child(std::move(submenu_uniq)));
	submenu->m_parent_menu = this;

	FloatingConfig fc = submenu->get_floating();
	fc.z_index = static_cast<int16_t>(get_floating().z_index + 1);
	submenu->set_floating(fc);

	m_items.push_back({ std::move(text), {}, icon, {}, submenu, false });
	return submenu;
}

void PopupMenu::clear_items() {
	close_submenu();
	for (View *v : m_item_views) {
		remove_child(v);
	}
	m_item_views.clear();
	for (auto &item : m_items) {
		if (item.submenu != nullptr) {
			remove_child(item.submenu);
		}
	}
	m_items.clear();
	invalidate_layout();
}

PopupMenu *PopupMenu::root_menu() {
	PopupMenu *menu = this;
	while (menu->m_parent_menu != nullptr) {
		menu = menu->m_parent_menu;
	}
	return menu;
}

void PopupMenu::open_submenu(PopupMenu *submenu, Vec2 at) {
	if (m_open_submenu == submenu) {
		return;
	}
	close_submenu();
	m_open_submenu = submenu;
	submenu->open_at(at);
}

void PopupMenu::close_submenu() {
	if (m_open_submenu != nullptr) {
		m_open_submenu->close_submenu();
		m_open_submenu->close();
		m_open_submenu = nullptr;
	}
}

void PopupMenu::dismiss() {
	close_submenu();
	close();
}

void PopupMenu::rebuild() {
	for (View *v : m_item_views) {
		remove_child(v);
	}
	m_item_views.clear();

	for (auto &item : m_items) {
		if (item.is_separator) {
			auto sep = std::make_unique<Separator>();
			sep->add_class("menu-separator");
			m_item_views.push_back(add_child(std::move(sep)));
			continue;
		}

		auto btn = std::make_unique<Button>();
		btn->set_reserve_icon_space(true);
		if (item.icon != nullptr) {
			btn->set_icon(item.icon);
		}
		btn->set_text(item.text);
		btn->add_class("menu-item");
		if (!item.shortcut.empty()) {
			btn->set_shortcut(item.shortcut);
		}
		if (item.submenu != nullptr) {
			btn->set_trailing_icon(root_menu()->m_submenu_icon);
		}

		auto *btn_ptr = dynamic_cast<Button *>(add_child(std::move(btn)));
		PopupMenu *submenu = item.submenu;

		btn_ptr->on_mouse_entered.connect([this, submenu, btn_ptr] {
			if (submenu != nullptr) {
				const Rect rect = btn_ptr->get_absolute_rect();
				open_submenu(submenu, { rect.position.x + rect.size.x, rect.position.y });
			} else {
				close_submenu();
			}
		});

		if (submenu != nullptr) {
			btn_ptr->on_click.connect([this, submenu, btn_ptr] {
				const Rect rect = btn_ptr->get_absolute_rect();
				open_submenu(submenu, { rect.position.x + rect.size.x, rect.position.y });
			});
		} else {
			btn_ptr->on_click.connect([this, cb = item.callback] {
				if (cb) {
					cb();
				}
				PopupMenu *root = root_menu();
				root->dismiss();
				if (root->m_on_activate) {
					root->m_on_activate();
				}
			});
		}

		m_item_views.push_back(btn_ptr);
	}

	invalidate_layout();
}

void PopupMenu::refresh() {
	rebuild();
}

void PopupMenu::open_at(Vec2 canvas_pos) {
	close_submenu();

	FloatingConfig fc = get_floating();
	fc.offset = canvas_pos;
	set_floating(fc);

	rebuild();
	open();
}

void PopupMenu::open_below(Vec2 anchor_pos, float anchor_height) {
	open_at({ anchor_pos.x, anchor_pos.y + anchor_height });
}

} // namespace Aquila::UI::Core
