#pragma once

#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/ScrollView.h"
#include "Aquila/UI/Widgets/Button.h"

namespace Aquila::UI::Core {

class ListBox : public View {
  public:
	ListBox();

	[[nodiscard]] std::string_view get_type_name() const override { return "ListBox"; }

	/// `id` is what selection uses, `display` is what's shown.
	void add_item(std::string id, std::string display);
	void remove_item(const std::string &id);
	void clear_items();

	void set_selected_id(const std::string &id);
	[[nodiscard]] const std::string &get_selected_id() const { return m_selected_id; }

	Signal<void(const std::string &)> on_selection_changed;

  private:
	void select_item(const std::string &id);
	void update_selection_styles();

	struct Item {
		std::string id;
		std::string display;
		Button *button = nullptr;
	};

	ScrollView *m_scroll = nullptr;
	std::vector<Item> m_items;
	std::string m_selected_id;
};

} // namespace Aquila::UI::Core
