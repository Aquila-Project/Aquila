#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/IResourceResolver.h"
#include "Aquila/UI/Core/MarkupItem.h"
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

void PopupMenu::insert_item(Usize index, std::string text, std::string shortcut, GFX::GfxTexture *icon,
							Delegate<void()> callback) {
	const auto at = static_cast<std::ptrdiff_t>(std::min(index, m_items.size()));
	m_items.insert(m_items.begin() + at,
				   { std::move(text), std::move(shortcut), icon, std::move(callback), nullptr, false });
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
	m_search_input = add_child(std::move(input));
	return m_search_input;
}

PopupMenu *PopupMenu::add_submenu(std::string text, GFX::GfxTexture *icon) {
	auto submenu_uniq = std::make_unique<PopupMenu>();
	auto *submenu = add_child(std::move(submenu_uniq));
	submenu->m_parent_menu = this;

	FloatingConfig fc = submenu->get_floating();
	fc.z_index = static_cast<int16_t>(get_floating().z_index + 1);
	submenu->set_floating(fc);

	m_items.push_back({ std::move(text), {}, icon, {}, submenu, false });
	return submenu;
}

bool PopupMenu::set_action(std::string_view action, Delegate<void()> callback) {
	bool found = false;
	for (Item &item : m_items) {
		if (item.submenu != nullptr) {
			found = item.submenu->set_action(action, callback) || found;
		} else if (!item.action.empty() && item.action == action) {
			item.callback = callback;
			found = true;
		}
	}
	return found;
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

		auto *btn_ptr = add_child(std::move(btn));
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

bool PopupMenu::add_markup_item(MarkupItem &item, IResourceResolver *resolver) {
	if (item.tag == "MenuSeparator") {
		add_separator();
		return true;
	}
	if (item.tag == "Submenu") {
		PopupMenu *submenu = add_submenu(std::string(item.get("label")), item.get_texture("icon", resolver));
		for (MarkupItem &entry : item.items) {
			submenu->add_markup_item(entry, resolver);
		}
		return true;
	}
	if (item.tag != "MenuItem") {
		return false;
	}

	std::string text(item.get("label"));
	std::string shortcut;
	Delegate<void()> callback;
	if (const std::string_view command = item.get("command"); !command.empty()) {
		const CommandInfo info = resolver != nullptr ? resolver->query_command(command) : CommandInfo{};
		if (info.known) {
			text = text.empty() ? info.title : text;
			shortcut = info.shortcut;
			callback = resolver->resolve_command(std::string(command));
		} else {
			AQUILA_LOG_WARNING("PopupMenu: {}: unknown command '{}'", item.location, command);
			text = text.empty() ? std::string(command) : text;
		}
	} else if (text.empty()) {
		AQUILA_LOG_WARNING("PopupMenu: {}: a <MenuItem> needs a command or a label", item.location);
		return true;
	}
	add_item(std::move(text), std::move(shortcut), item.get_texture("icon", resolver), std::move(callback));
	m_items.back().action = item.get("action");
	return true;
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

void PopupMenu::on_floating_layout() {
	const Canvas *canvas = get_canvas();
	if (canvas == nullptr) {
		return;
	}
	const Vec2 bounds(static_cast<F32>(canvas->get_width()), static_cast<F32>(canvas->get_height()));
	const Vec2 size = get_layout_rect().size;
	const Vec2 placed = get_absolute_position();
	Vec2 fitted = placed;
	if (m_parent_menu != nullptr && placed.x + size.x > bounds.x) {
		fitted.x = m_parent_menu->get_absolute_position().x - size.x;
	}
	fitted.x = std::clamp(fitted.x, 0.F, std::max(0.F, bounds.x - size.x));
	fitted.y = std::clamp(fitted.y, 0.F, std::max(0.F, bounds.y - size.y));

	constexpr F32 K_TOLERANCE = 0.01F;
	if (std::abs(fitted.x - placed.x) < K_TOLERANCE && std::abs(fitted.y - placed.y) < K_TOLERANCE) {
		return;
	}
	FloatingConfig floating = get_floating();
	floating.offset += fitted - placed;
	set_floating(floating);
	invalidate_layout();
}

} // namespace Aquila::UI::Core
