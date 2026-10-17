#include "Aquila/UI/Widgets/Menubar.h"

namespace Aquila::UI::Core {

MenuBar::MenuBar() {
	add_class("menu-bar");
}

PopupMenu *MenuBar::add_menu(std::string title) {
	auto btn = std::make_unique<Button>();
	btn->set_text(std::move(title));
	btn->add_class("menu-bar-item");

	auto dropdown = std::make_unique<PopupMenu>();
	PopupMenu *dropdown_ptr = dropdown.get();
	dropdown_ptr->set_on_activate([this] { close_all(); });

	btn->on_click.connect([this, dropdown_ptr] {
		if (m_open_dropdown == dropdown_ptr) {
			close_all();
			return;
		}
		for (auto &entry : m_entries) {
			if (entry.dropdown == dropdown_ptr) {
				Rect rect = entry.button->get_absolute_rect();
				open_dropdown(dropdown_ptr, rect.position, rect.size.y);
				return;
			}
		}
	});

	Button *btn_ptr = dynamic_cast<Button *>(add_child(std::move(btn)));
	View *dropdown_parent = (m_overlay_root != nullptr) ? m_overlay_root : static_cast<View *>(this);
	dropdown_parent->add_child(std::move(dropdown));

	m_entries.push_back({ .button = btn_ptr, .dropdown = dropdown_ptr });
	return dropdown_ptr;
}

void MenuBar::set_overlay_root(View *root) {
	m_overlay_root = root;
}

void MenuBar::close_all() {
	if (m_open_dropdown != nullptr) {
		m_open_dropdown->dismiss();
		m_open_dropdown = nullptr;
	}
}

void MenuBar::open_dropdown(PopupMenu *dropdown, Vec2 button_abs_pos, float button_height) {
	if ((m_open_dropdown != nullptr) && m_open_dropdown != dropdown) {
		m_open_dropdown->dismiss();
	}
	m_open_dropdown = dropdown;
	dropdown->open_below(button_abs_pos, button_height);
}

} // namespace Aquila::UI::Core
