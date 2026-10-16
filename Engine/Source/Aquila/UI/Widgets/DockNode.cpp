#include "Aquila/UI/Widgets/DockNode.h"
#include "Aquila/UI/Widgets/DockCloseButton.h"
#include "Aquila/UI/Widgets/DockPanel.h"
#include "Aquila/UI/Widgets/DockSplitter.h"
#include "Aquila/UI/Widgets/DockTabButton.h"

#include "Aquila/UI/Style/StyleTypes.h"

namespace Aquila::UI::Core {

View *DockNode::make_zone_indicator(FloatingAttachPoint elem_pt, FloatingAttachPoint parent_pt, Vec2 offset,
									const char *cls) {
	auto zone = std::make_unique<View>();
	zone->add_class("dock-zone-indicator");
	zone->add_class(cls);

	FloatingConfig cfg;
	cfg.attach_to = FloatingAttachTo::Parent;
	cfg.element_point = elem_pt;
	cfg.parent_point = parent_pt;
	cfg.offset = offset;
	cfg.z_index = 50;
	zone->set_floating(cfg);
	zone->set_hidden(true);

	return add_child(std::move(zone));
}

DockNode::DockNode(DockDragContext *drag_ctx) : m_drag_ctx(drag_ctx) {
	add_class("dock-node");
	add_class("dock-leaf");

	auto tab_bar = std::make_unique<View>();
	tab_bar->add_class("dock-tab-bar");
	m_tab_bar = add_child(std::move(tab_bar));

	auto tab_strip = std::make_unique<View>();
	tab_strip->add_class("dock-tab-strip");
	m_tab_strip = m_tab_bar->add_child(std::move(tab_strip));

	auto tab_actions = std::make_unique<View>();
	tab_actions->add_class("dock-tab-actions");
	m_tab_actions = m_tab_bar->add_child(std::move(tab_actions));

	auto panel_area = std::make_unique<View>();
	panel_area->add_class("dock-panel-area");
	m_panel_area = add_child(std::move(panel_area));

	using AP = FloatingAttachPoint;
	constexpr float k_step = 52.F;
	m_zone_center = make_zone_indicator(AP::Center, AP::Center, { 0.F, 0.F }, "dock-zone-center-ind");
	m_zone_left = make_zone_indicator(AP::Center, AP::Center, { -k_step, 0.F }, "dock-zone-left-ind");
	m_zone_right = make_zone_indicator(AP::Center, AP::Center, { k_step, 0.F }, "dock-zone-right-ind");
	m_zone_top = make_zone_indicator(AP::Center, AP::Center, { 0.F, -k_step }, "dock-zone-top-ind");
	m_zone_bottom = make_zone_indicator(AP::Center, AP::Center, { 0.F, k_step }, "dock-zone-bottom-ind");

	decorate_tab_bar();
}

void DockNode::decorate_tab_bar() {
	if (m_tab_actions == nullptr || m_drag_ctx == nullptr || !m_drag_ctx->decorate_tab_bar) {
		return;
	}
	while (!m_tab_actions->get_children().empty()) {
		m_tab_actions->remove_child(m_tab_actions->get_children().back().get());
	}
	m_drag_ctx->decorate_tab_bar(this, m_tab_actions);
}

std::pair<DockNode *, DockNode *> DockNode::split(SplitDirection dir, bool anchor_first) {
	for (View *z : { m_zone_center, m_zone_left, m_zone_right, m_zone_top, m_zone_bottom }) {
		if (z != nullptr) {
			remove_child(z);
		}
	}
	m_zone_center = m_zone_left = m_zone_right = m_zone_top = m_zone_bottom = nullptr;

	if (m_tab_bar != nullptr) {
		remove_child(m_tab_bar);
		m_tab_bar = nullptr;
		m_tab_strip = nullptr;
		m_tab_actions = nullptr;
	}
	if (m_panel_area != nullptr) {
		remove_child(m_panel_area);
		m_panel_area = nullptr;
	}
	m_is_leaf = false;
	remove_class("dock-leaf");
	add_class("dock-split");

	StyleProperties sp;
	sp.flex_direction = (dir == SplitDirection::Horizontal) ? FlexDirection::Row : FlexDirection::Column;
	merge_style(sp);

	const bool is_h = (dir == SplitDirection::Horizontal);

	auto first = std::make_unique<DockNode>(m_drag_ctx);
	{
		StyleProperties fp;

		if (is_h) {
			fp.height = StyleLength::percent(100.F);
		} else {
			fp.width = StyleLength::percent(100.F);
		}
		first->merge_style(fp);
	}

	auto splitter = std::make_unique<DockSplitter>(dir);
	splitter->set_resize_before(anchor_first);

	auto second = std::make_unique<DockNode>(m_drag_ctx);
	{
		StyleProperties sp2;
		sp2.flex_grow = 1.F;
		if (is_h) {
			sp2.height = StyleLength::percent(100.F);
		} else {
			sp2.width = StyleLength::percent(100.F);
		}
		second->merge_style(sp2);
	}

	DockNode *first_raw = dynamic_cast<DockNode *>(add_child(std::move(first)));
	DockNode *second_raw = dynamic_cast<DockNode *>(add_child(std::move(second)));
	second_raw->add_child(std::move(splitter));

	return { first_raw, second_raw };
}

DockNode *DockNode::append_leaf(SplitDirection dir) {
	DockNode *prev_last = nullptr;
	for (auto &child : get_children()) {
		if (auto *dn = view_cast<DockNode>(child.get())) {
			prev_last = dn;
		}
	}
	if (prev_last == nullptr) {
		return nullptr;
	}

	const bool is_h = (dir == SplitDirection::Horizontal);

	auto splitter = std::make_unique<DockSplitter>(dir);
	splitter->set_resize_before(false);

	auto leaf = std::make_unique<DockNode>(m_drag_ctx);
	{
		StyleProperties lp;
		lp.flex_grow = 0.F;
		if (is_h) {
			lp.height = StyleLength::percent(100.F);
		} else {
			lp.width = StyleLength::percent(100.F);
		}
		leaf->merge_style(lp);
	}

	DockNode *leaf_raw = dynamic_cast<DockNode *>(add_child(std::move(leaf)));
	leaf_raw->add_child(std::move(splitter));

	return leaf_raw;
}

void DockNode::append_tab(DockPanel *panel, std::string title) {
	auto wrapper = std::make_unique<View>();
	wrapper->add_class("dock-tab-wrapper");
	View *wrapper_raw = m_tab_strip->add_child(std::move(wrapper));

	auto btn = std::make_unique<DockTabButton>();
	btn->set_text(title);
	if (panel != nullptr && panel->get_tab_icon() != nullptr) {
		btn->set_icon(panel->get_tab_icon());
	}
	btn->add_class("dock-tab-btn");
	DockTabButton *btn_raw = dynamic_cast<DockTabButton *>(wrapper_raw->add_child(std::move(btn)));

	btn_raw->set_drag_info(m_drag_ctx, panel, this);
	btn_raw->on_click.connect([this, panel] { set_active_panel_by_ptr(panel); });

	if (m_drag_ctx != nullptr && m_drag_ctx->closable_tabs) {
		auto close = std::make_unique<DockCloseButton>();
		close->add_class("dock-tab-close-btn");
		if (m_drag_ctx->close_icon != nullptr) {
			close->set_icon(m_drag_ctx->close_icon);
		}
		close->set_close_info(this, panel);
		wrapper_raw->add_child(std::move(close));
	}

	m_tabs.push_back({ wrapper_raw, btn_raw, panel, std::move(title) });
}

DockPanel *DockNode::add_panel(std::string title, GFX::GfxTexture *tab_icon) {
	auto panel = std::make_unique<DockPanel>(title);
	DockPanel *panel_raw = dynamic_cast<DockPanel *>(m_panel_area->add_child(std::move(panel)));
	panel_raw->set_tab_icon(tab_icon);

	append_tab(panel_raw, std::move(title));

	if (m_active_panel < 0) {
		set_active_panel(0);
	}

	remove_class("dock-node-empty");

	return panel_raw;
}

void DockNode::apply_xml_attribute(std::string_view name, std::string_view value, IResourceResolver *resolver) {
	if (name == "split") {
		if (value == "horizontal" || value == "row") {
			m_declared_split = SplitDirection::Horizontal;
		} else if (value == "vertical" || value == "column") {
			m_declared_split = SplitDirection::Vertical;
		}
		return;
	}
	View::apply_xml_attribute(name, value, resolver);
}

void DockNode::set_active_panel(int index) {
	if (index < 0 || index >= static_cast<int>(m_tabs.size())) {
		return;
	}
	m_active_panel = index;
	apply_active_panel();
}

void DockNode::set_active_panel_by_ptr(DockPanel *panel) {
	for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
		if (m_tabs[i].panel == panel) {
			set_active_panel(i);
			return;
		}
	}
}

void DockNode::apply_active_panel() {
	for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
		const bool active = (i == m_active_panel);
		m_tabs[i].panel->set_hidden(!active);
		if (active) {
			m_tabs[i].wrapper->add_class("dock-tab-active");
			m_tabs[i].button->add_class("dock-tab-btn-active");
		} else {
			m_tabs[i].wrapper->remove_class("dock-tab-active");
			m_tabs[i].button->remove_class("dock-tab-btn-active");
		}
	}
}

int DockNode::index_of(const DockPanel *panel) const {
	for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
		if (m_tabs[i].panel == panel) {
			return i;
		}
	}
	return -1;
}

Unique<View> DockNode::replace_panel(DockPanel *old_panel, Unique<View> new_view) {
	const int index = index_of(old_panel);
	auto *new_panel = view_cast<DockPanel>(new_view.get());
	if (index < 0 || new_panel == nullptr) {
		return nullptr;
	}

	std::string title = new_panel->get_title();
	m_tab_strip->remove_child(m_tabs[index].wrapper);
	Unique<View> old_owned = m_panel_area->detach_child(old_panel);
	m_panel_area->add_child(std::move(new_view));

	const int active = m_active_panel;
	m_tabs.erase(m_tabs.begin() + index);
	append_tab(new_panel, std::move(title));
	std::rotate(m_tabs.begin() + index, m_tabs.end() - 1, m_tabs.end());

	std::vector<Unique<View>> wrappers;
	wrappers.reserve(m_tabs.size());
	for (auto &t : m_tabs) {
		wrappers.push_back(m_tab_strip->detach_child(t.wrapper));
	}
	for (size_t i = 0; i < m_tabs.size(); ++i) {
		m_tabs[i].wrapper = m_tab_strip->add_child(std::move(wrappers[i]));
	}

	m_active_panel = active;
	apply_active_panel();
	return old_owned;
}

std::vector<DockPanel *> DockNode::get_ordered_panels() const {
	std::vector<DockPanel *> panels;
	panels.reserve(m_tabs.size());
	for (const auto &tab : m_tabs) {
		panels.push_back(tab.panel);
	}
	return panels;
}

DockPanel *DockNode::get_active_panel_ptr() const {
	if (m_active_panel < 0 || m_active_panel >= static_cast<int>(m_tabs.size())) {
		return nullptr;
	}

	return m_tabs[m_active_panel].panel;
}

Unique<View> DockNode::detach_panel(DockPanel *panel) {
	auto it = std::ranges::find_if(m_tabs, [panel](const Tab &t) { return t.panel == panel; });
	if (it == m_tabs.end()) {
		return nullptr;
	}

	m_tab_strip->remove_child(it->wrapper);
	auto owned = m_panel_area->detach_child(panel);
	m_tabs.erase(it);

	if (m_tabs.empty()) {
		m_active_panel = -1;
		add_class("dock-node-empty");
	} else {
		m_active_panel = std::clamp(m_active_panel, 0, static_cast<int>(m_tabs.size()) - 1);
		apply_active_panel();
	}

	return owned;
}

void DockNode::close_panel(DockPanel *panel) {
	auto owned = detach_panel(panel);
	// `owned` destroyed at end of scope — panel is gone.

	if (m_tabs.empty() && (m_drag_ctx != nullptr) && m_drag_ctx->on_node_emptied) {
		m_drag_ctx->on_node_emptied(this);
	}
}

void DockNode::reorder_panel(DockPanel *panel, Vec2 cursor_pos) {
	auto it = std::ranges::find_if(m_tabs, [panel](const Tab &t) { return t.panel == panel; });
	if (it == m_tabs.end()) {
		return;
	}

	int target_index = static_cast<int>(m_tabs.size()) - 1;
	for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
		const Rect r = m_tabs[i].wrapper->get_absolute_rect();
		if (cursor_pos.x <= r.position.x + r.size.x * 0.5F) {
			target_index = i;
			break;
		}
	}

	int source_index = static_cast<int>(std::distance(m_tabs.begin(), it));
	if (source_index == target_index) {
		return;
	}

	if (source_index < target_index) {
		std::rotate(m_tabs.begin() + source_index, m_tabs.begin() + source_index + 1,
					m_tabs.begin() + target_index + 1);
	} else {
		std::rotate(m_tabs.begin() + target_index, m_tabs.begin() + source_index, m_tabs.begin() + source_index + 1);
	}

	if (m_active_panel == source_index) {
		m_active_panel = target_index;
	} else if (source_index < target_index) {
		if (m_active_panel > source_index && m_active_panel <= target_index) {
			m_active_panel--;
		}
	} else {
		if (m_active_panel >= target_index && m_active_panel < source_index) {
			m_active_panel++;
		}
	}

	std::vector<Unique<View>> wrappers;
	wrappers.reserve(m_tabs.size());
	for (auto &t : m_tabs) {
		wrappers.push_back(m_tab_strip->detach_child(t.wrapper));
	}
	for (size_t i = 0; i < m_tabs.size(); ++i) {
		m_tabs[i].wrapper = m_tab_strip->add_child(std::move(wrappers[i]));
	}

	apply_active_panel();
}

void DockNode::accept_panel(Unique<View> panel_view, std::string title, DropZone zone) {
	if (zone == DropZone::None) {
		return;
	}

	if (zone == DropZone::Center) {
		DockPanel *panel_raw = dynamic_cast<DockPanel *>(m_panel_area->add_child(std::move(panel_view)));
		append_tab(panel_raw, std::move(title));
		set_active_panel(static_cast<int>(m_tabs.size()) - 1);
		remove_class("dock-node-empty");
		return;
	}

	struct Saved {
		Unique<View> view;
		std::string title;
	};
	std::vector<Saved> existing;
	existing.reserve(m_tabs.size());

	for (auto &tab : m_tabs) {
		m_tab_strip->remove_child(tab.wrapper);
		existing.push_back({ m_panel_area->detach_child(tab.panel), tab.title });
	}
	m_tabs.clear();
	m_active_panel = -1;

	const SplitDirection split_dir =
		(zone == DropZone::Left || zone == DropZone::Right) ? SplitDirection::Horizontal : SplitDirection::Vertical;
	const bool new_first = (zone == DropZone::Left || zone == DropZone::Top);

	auto [firstNode, secondNode] = split(split_dir);

	DockNode *new_node = new_first ? firstNode : secondNode;
	DockNode *keep_node = new_first ? secondNode : firstNode;

	new_node->accept_panel(std::move(panel_view), std::move(title), DropZone::Center);
	for (auto &saved : existing) {
		keep_node->accept_panel(std::move(saved.view), std::move(saved.title), DropZone::Center);
	}
}

DockNode *DockNode::hit_test_node(Vec2 abs_pos) {
	if (m_is_leaf) {
		return get_absolute_rect().contains(abs_pos) ? this : nullptr;
	}
	for (auto &child : get_children()) {
		if (auto *dn = view_cast<DockNode>(child.get())) {
			if (auto *hit = dn->hit_test_node(abs_pos)) {
				return hit;
			}
		}
	}
	return nullptr;
}

void DockNode::show_drop_zones(bool show) {
	if (!m_is_leaf) {
		return;
	}
	for (View *z : { m_zone_center, m_zone_left, m_zone_right, m_zone_top, m_zone_bottom }) {
		if (z != nullptr) {
			z->set_hidden(!show);
		}
	}
	if (show) {
		add_class("dock-drop-target");
	} else {
		remove_class("dock-drop-target");
	}
}

void DockNode::highlight_drop_zone(DropZone zone) {
	for (View *z : { m_zone_center, m_zone_left, m_zone_right, m_zone_top, m_zone_bottom }) {
		if (z != nullptr) {
			z->remove_class("dock-zone-indicator-active");
		}
	}
	View *active = nullptr;
	switch (zone) {
	case DropZone::Center:
		active = m_zone_center;
		break;
	case DropZone::Left:
		active = m_zone_left;
		break;
	case DropZone::Right:
		active = m_zone_right;
		break;
	case DropZone::Top:
		active = m_zone_top;
		break;
	case DropZone::Bottom:
		active = m_zone_bottom;
		break;
	default:
		break;
	}
	if (active != nullptr) {
		active->add_class("dock-zone-indicator-active");
	}
}

DropZone DockNode::hit_test_drop_zone(Vec2 abs_pos) const {
	// Precise: explicit indicator boxes take priority.
	if ((m_zone_center != nullptr) && m_zone_center->get_absolute_rect().contains(abs_pos)) {
		return DropZone::Center;
	}
	if ((m_zone_left != nullptr) && m_zone_left->get_absolute_rect().contains(abs_pos)) {
		return DropZone::Left;
	}
	if ((m_zone_right != nullptr) && m_zone_right->get_absolute_rect().contains(abs_pos)) {
		return DropZone::Right;
	}
	if ((m_zone_top != nullptr) && m_zone_top->get_absolute_rect().contains(abs_pos)) {
		return DropZone::Top;
	}
	if ((m_zone_bottom != nullptr) && m_zone_bottom->get_absolute_rect().contains(abs_pos)) {
		return DropZone::Bottom;
	}

	// explicit center indicator above — the neutral interior returns None so a release there tears
	const Rect r = get_absolute_rect();
	if (!r.contains(abs_pos)) {
		return DropZone::None;
	}

	const float rel_x = (abs_pos.x - r.position.x) / r.size.x;
	const float rel_y = (abs_pos.y - r.position.y) / r.size.y;
	constexpr float k_edge = 0.25F;

	if (rel_x < k_edge) {
		return DropZone::Left;
	}
	if (rel_x > 1.F - k_edge) {
		return DropZone::Right;
	}
	if (rel_y < k_edge) {
		return DropZone::Top;
	}
	if (rel_y > 1.F - k_edge) {
		return DropZone::Bottom;
	}
	return DropZone::None;
}

} // namespace Aquila::UI::Core
