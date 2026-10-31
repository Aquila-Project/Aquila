#include "Aquila/UI/Widgets/ListBox.h"

#include "Aquila/UI/Core/MarkupItem.h"

namespace Aquila::UI::Core {

ListBox::ListBox() {
	add_class("list-box");

	auto scroll = std::make_unique<ScrollView>();
	scroll->add_class("list-box-scroll");
	m_scroll = add_child(std::move(scroll));
}

void ListBox::add_item(std::string id, std::string display) {
	auto btn = std::make_unique<Button>(display);
	btn->add_class("list-item");
	btn->on_click.connect([this, id] { select_item(id); });

	Button *btn_ptr = m_scroll->add_child(std::move(btn));
	m_items.push_back({ std::move(id), std::move(display), btn_ptr });
}

bool ListBox::add_markup_item(MarkupItem &item, IResourceResolver *) {
	if (item.tag != "Item") {
		return false;
	}
	const std::string_view value = item.get("value");
	const std::string_view label = item.get("label");
	if (value.empty() && label.empty()) {
		AQUILA_LOG_WARNING("ListBox: {}: an <Item> needs a value or a label", item.location);
		return true;
	}
	add_item(std::string(value.empty() ? label : value), std::string(label.empty() ? value : label));
	return true;
}

void ListBox::remove_item(const std::string &id) {
	auto it = std::ranges::find_if(m_items, [&](const Item &item) { return item.id == id; });
	if (it == m_items.end()) {
		return;
	}

	if (it->button->get_parent() != nullptr) {
		it->button->get_parent()->remove_child(it->button);
	}

	if (m_selected_id == it->id) {
		m_selected_id.clear();
	}
	m_items.erase(it);
}

void ListBox::clear_items() {
	for (auto &item : m_items) {
		View *parent = item.button->get_parent();
		if (parent != nullptr) {
			parent->remove_child(item.button);
		}
	}
	m_items.clear();
	m_selected_id.clear();
}

void ListBox::set_selected_id(const std::string &id) {
	m_selected_id = id;
	update_selection_styles();
}

void ListBox::select_item(const std::string &id) {
	if (m_selected_id == id) {
		return;
	}
	m_selected_id = id;
	update_selection_styles();
	on_selection_changed(m_selected_id);
}

void ListBox::update_selection_styles() {
	for (auto &item : m_items) {
		if (item.id == m_selected_id) {
			item.button->add_class("selected");
		} else {
			item.button->remove_class("selected");
		}
	}
}

} // namespace Aquila::UI::Core
