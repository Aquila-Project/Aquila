#include "Aquila/UI/Widgets/DockSpace.h"

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Math/Rect.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/DockLayoutSerializer.h"
#include "Aquila/UI/Style/StyleTypes.h"
#include "Aquila/UI/Widgets/DockNode.h"
#include "Aquila/UI/Widgets/DockPanel.h"
#include "Aquila/UI/Widgets/DockSplitter.h"

namespace Aquila::UI::Core {

DockSpace::DockSpace() {
	add_class("dock-space");

	auto preview = std::make_unique<View>();
	preview->add_class("dock-drop-preview");
	{
		FloatingConfig cfg;
		cfg.attach_to = FloatingAttachTo::Root;
		cfg.element_point = FloatingAttachPoint::LeftTop;
		cfg.parent_point = FloatingAttachPoint::LeftTop;
		cfg.z_index = 30;
		preview->set_floating(cfg);
	}
	preview->set_hidden(true);
	m_drop_preview = add_child(std::move(preview));

	m_drag_ctx.on_node_emptied = [this](DockNode *node) {
		collapse_node(node);
		if (!has_any_panels() && m_on_emptied) {
			m_on_emptied();
		}
	};

	m_drag_ctx.on_move = [this](Vec2 pos) {
		update_drag_ghost(pos);

		if (is_outside_canvas(pos)) {
			if (m_drop_target) {
				m_drop_target->show_drop_zones(false);
				m_drop_target->highlight_drop_zone(DropZone::None);
				m_drop_target = nullptr;
			}
			update_preview(nullptr, DropZone::None);
			m_current_zone = DropZone::None;
			m_drag_left_canvas = true;
			if (m_on_external_drag_move) {
				m_on_external_drag_move(pos);
			}
			return;
		}
		if (m_drag_left_canvas) {
			m_drag_left_canvas = false;
			if (m_on_external_drag_clear) {
				m_on_external_drag_clear();
			}
		}

		DockNode *hovered = m_root->hit_test_node(pos);
		bool is_self_drag = (hovered != nullptr && hovered == m_drag_ctx.source_node);

		if (is_self_drag) {
			View *bar = m_drag_ctx.source_node ? m_drag_ctx.source_node->get_tab_bar() : nullptr;
			const bool over_bar = bar && bar->get_absolute_rect().contains(pos);
			if (!over_bar || m_drag_ctx.source_node->get_tab_count() < 2) {
				hovered = nullptr;
				is_self_drag = false;
			}
		}

		if (hovered != m_drop_target) {
			if (m_drop_target) {
				m_drop_target->show_drop_zones(false);
				m_drop_target->highlight_drop_zone(DropZone::None);
			}
			update_preview(nullptr, DropZone::None);
			m_drop_target = hovered;
			if (m_drop_target && !is_self_drag) {
				m_drop_target->show_drop_zones(true);
			}
			m_current_zone = DropZone::None;
		}

		if (!m_drop_target) {
			return;
		}

		if (is_self_drag) {
			// Reorder mode — no zone indicators, zone locked to Center.
			m_current_zone = DropZone::Center;
		} else {
			DropZone zone = m_drop_target->hit_test_drop_zone(pos);
			if (zone != m_current_zone) {
				m_drop_target->highlight_drop_zone(zone);
				m_current_zone = zone;
				update_preview(m_drop_target, zone);
			}
		}
	};

	m_drag_ctx.on_release = [this](Vec2 pos) {
		hide_drag_ghost();

		if (m_drag_left_canvas) {
			m_drag_left_canvas = false;
			if (m_on_external_drag_clear) {
				m_on_external_drag_clear();
			}
		}
		const bool has_valid_drop = (m_drop_target && m_drag_ctx.active && m_current_zone != DropZone::None);

		if (m_drop_target) {
			m_drop_target->show_drop_zones(false);
			m_drop_target->highlight_drop_zone(DropZone::None);
		}
		update_preview(nullptr, DropZone::None);

		if (has_valid_drop) {
			execute_drop(m_drop_target, m_current_zone, pos);
		} else if (m_drag_ctx.active && m_drag_ctx.panel && m_drag_ctx.source_node && m_on_tear_off) {
			// No dock zone under the cursor. If the tab was pulled off its bar, tear it off — the
			View *bar = m_drag_ctx.source_node->get_tab_bar();
			const bool left_bar = !bar || !bar->get_absolute_rect().contains(pos);
			if (left_bar) {
				DockNode *source = m_drag_ctx.source_node;
				DockPanel *panel = m_drag_ctx.panel;
				std::string title(panel->get_title());
				auto panel_view = source->detach_panel(panel);
				if (panel_view) {
					if (source->is_empty()) {
						collapse_node(source);
					}
					m_on_tear_off(std::move(panel_view), std::move(title), pos);
				}
			}
		}

		m_drop_target = nullptr;
		m_current_zone = DropZone::None;
		m_drag_ctx.active = false;
		m_drag_ctx.panel = nullptr;
		m_drag_ctx.source_node = nullptr;
	};

	auto root = std::make_unique<DockNode>(&m_drag_ctx);
	m_root = dynamic_cast<DockNode *>(add_child(std::move(root)));
}

namespace {

std::vector<View *> declared_dock_children(View *node) {
	std::vector<View *> out;
	for (const auto &child : node->get_children()) {
		View *c = child.get();
		if (view_is<DockNode>(c) || view_is<DockPanel>(c)) {
			out.push_back(c);
		}
	}
	return out;
}

} // namespace

void DockSpace::on_xml_loaded() {
	DockNode *decl_root = nullptr;
	for (const auto &child : get_children()) {
		auto *dn = view_cast<DockNode>(child.get());
		if (dn != nullptr && dn != m_root) {
			decl_root = dn;
			break;
		}
	}
	if (decl_root == nullptr) {
		return; // no declarative layout — keep the default empty root
	}

	compile_declaration(m_root, decl_root);
	remove_child(decl_root);
}

void DockSpace::compile_declaration(DockNode *real_node, DockNode *decl_node) {
	const Option<SplitDirection> split = decl_node->get_declared_split();
	const std::vector<View *> slots = declared_dock_children(decl_node);

	if (!split.has_value()) {
		for (View *slot : slots) {
			if (auto *panel = view_cast<DockPanel>(slot)) {
				realize_panel(real_node, panel);
			}
		}
		return;
	}

	if (slots.empty()) {
		return;
	}
	if (slots.size() == 1) {
		realize_slot(real_node, slots[0]);
		return;
	}

	std::vector<DockNode *> leaves;
	auto [first, second] = real_node->split(*split);
	leaves.push_back(first);
	leaves.push_back(second);
	for (size_t i = 2; i < slots.size(); ++i) {
		DockNode *leaf = real_node->append_leaf(*split);
		if (leaf == nullptr) {
			break;
		}
		leaves.push_back(leaf);
	}

	for (size_t i = 0; i < leaves.size(); ++i) {
		realize_slot(leaves[i], slots[i]);
	}
}

void DockSpace::realize_slot(DockNode *real_leaf, View *slot) {
	if (auto *child_node = view_cast<DockNode>(slot)) {
		compile_declaration(real_leaf, child_node);
	} else if (auto *panel = view_cast<DockPanel>(slot)) {
		realize_panel(real_leaf, panel);
	}
}

void DockSpace::realize_panel(DockNode *real_leaf, DockPanel *decl_panel) {
	DockPanel *real_panel = real_leaf->add_panel(decl_panel->get_title(), decl_panel->get_tab_icon());
	real_panel->set_id(decl_panel->get_id());
	for (const auto &cls : decl_panel->get_classes()) {
		real_panel->add_class(cls);
	}

	std::vector<View *> content;
	for (const auto &child : decl_panel->get_children()) {
		content.push_back(child.get());
	}
	for (View *child : content) {
		real_panel->add_child(decl_panel->detach_child(child));
	}
}

static DockNode *find_leaf_with_tabs(View *view) {
	if (auto *node = view_cast<DockNode>(view)) {
		if (node->get_tab_count() > 0) {
			return node;
		}
	}
	for (const auto &child : view->get_children()) {
		if (DockNode *found = find_leaf_with_tabs(child.get())) {
			return found;
		}
	}
	return nullptr;
}

bool DockSpace::has_any_panels() const {
	return (m_root != nullptr) && find_leaf_with_tabs(m_root) != nullptr;
}

DockNode *DockSpace::first_leaf_with_tabs() const {
	return (m_root != nullptr) ? find_leaf_with_tabs(m_root) : nullptr;
}

void DockSpace::update_preview(DockNode *target, DropZone zone) {
	if (m_drop_preview == nullptr) {
		return;
	}

	if ((target == nullptr) || zone == DropZone::None) {
		m_drop_preview->set_hidden(true);
		return;
	}

	Rect r = target->get_absolute_rect();
	Rect preview = r;

	switch (zone) {
	case DropZone::Center:

		break;
	case DropZone::Left:
		preview.size.x *= 0.5F;
		break;
	case DropZone::Right:
		preview.position.x += r.size.x * 0.5F;
		preview.size.x *= 0.5F;
		break;
	case DropZone::Top:
		preview.size.y *= 0.5F;
		break;
	case DropZone::Bottom:
		preview.position.y += r.size.y * 0.5F;
		preview.size.y *= 0.5F;
		break;
	default:
		break;
	}

	FloatingConfig cfg;
	cfg.attach_to = FloatingAttachTo::Root;
	cfg.element_point = FloatingAttachPoint::LeftTop;
	cfg.parent_point = FloatingAttachPoint::LeftTop;
	cfg.offset = preview.position;
	cfg.z_index = 30;
	m_drop_preview->set_floating(cfg);
	m_drop_preview->invalidate_layout();

	StyleProperties sp;
	sp.width = StyleLength::pixel(preview.size.x);
	sp.height = StyleLength::pixel(preview.size.y);
	m_drop_preview->merge_style(sp);
	m_drop_preview->set_hidden(false);
}

void DockSpace::update_drag_ghost(Vec2 pos) {
	Canvas *canvas = get_canvas();
	if ((canvas == nullptr) || !m_drag_ctx.active) {
		return;
	}

	if (!m_drag_ghost_active) {
		std::string title = (m_drag_ctx.panel != nullptr) ? m_drag_ctx.panel->get_title() : m_drag_ctx.title;
		GFX::GfxTexture *icon = (m_drag_ctx.panel != nullptr) ? m_drag_ctx.panel->get_tab_icon() : nullptr;
		canvas->show_drag_ghost(std::move(title), pos, icon);
		m_drag_ghost_active = true;
		return;
	}

	canvas->move_drag_ghost(pos);
}

void DockSpace::hide_drag_ghost() {
	m_drag_ghost_active = false;
	if (Canvas *canvas = get_canvas()) {
		canvas->hide_drag_ghost();
	}
}

void DockSpace::execute_drop(DockNode *target, DropZone zone, Vec2 release_pos) {
	if (target == nullptr) {
		return;
	}

	DockNode *source = m_drag_ctx.source_node;

	if (source == nullptr) {
		if (!m_drag_ctx.external_view) {
			return;
		}
		Unique<View> panel_view = std::move(m_drag_ctx.external_view);
		target->accept_panel(std::move(panel_view), m_drag_ctx.title, zone);
		return;
	}

	DockPanel *panel = m_drag_ctx.panel;
	if (panel == nullptr) {
		return;
	}

	const bool self_drop = (target == source);

	if (self_drop && zone == DropZone::Center) {
		source->reorder_panel(panel, release_pos);
		return;
	}

	if (self_drop && source->get_tab_count() < 2) {
		return;
	}

	std::string title = std::string(panel->get_title());

	auto panel_view = source->detach_panel(panel);
	if (!panel_view) {
		return;
	}

	if (!self_drop && source->is_empty()) {
		collapse_node(source);
	}

	target->accept_panel(std::move(panel_view), std::move(title), zone);
}

void DockSpace::collapse_node(DockNode *node) {
	if (node == m_root) {
		return;
	}

	auto *parent = view_cast<DockNode>(node->get_parent());
	if (parent == nullptr) {
		return;
	}

	DockNode *prev = nullptr;
	DockNode *next = nullptr;
	bool seen = false;
	for (const auto &child : parent->get_children()) {
		auto *dn = view_cast<DockNode>(child.get());
		if (dn == nullptr) {
			continue;
		}
		if (dn == node) {
			seen = true;
		} else if (!seen) {
			prev = dn;
		} else if (next == nullptr) {
			next = dn;
		}
	}

	if (prev == nullptr && next != nullptr) {
		for (const auto &child : next->get_children()) {
			if (auto *ds = view_cast<DockSplitter>(child.get())) {
				next->remove_child(ds);
				break;
			}
		}
	}

	parent->remove_child(node);

	DockNode *survivor = nullptr;
	if (next == nullptr) {
		survivor = prev;
	} else if (prev == nullptr) {
		survivor = next;
	}

	if (survivor != nullptr) {
		StyleProperties sp;
		sp.width = StyleLength::grow();
		sp.height = StyleLength::grow();
		sp.flex_grow = 1.F;
		survivor->merge_style(sp);
	}

	DockNode *only_child = nullptr;
	int node_count = 0;
	for (const auto &child : parent->get_children()) {
		if (auto *dn = view_cast<DockNode>(child.get())) {
			++node_count;
			only_child = dn;
		}
	}
	if (node_count == 1 && (only_child != nullptr)) {
		hoist_single_child(parent, only_child);
	} else {
		parent->invalidate_layout();
	}
}

void DockSpace::hoist_single_child(DockNode *container, DockNode *only) {
	View *grandparent = container->get_parent();
	Unique<View> owned = container->detach_child(only);
	if (!owned) {
		return;
	}

	const StyleProperties &slot = container->get_style();
	StyleProperties sp;
	sp.width = slot.width.value_or(StyleLength::grow());
	sp.height = slot.height.value_or(StyleLength::grow());
	sp.flex_grow = slot.flex_grow.value_or(1.F);
	only->merge_style(sp);

	for (const auto &child : only->get_children()) {
		if (auto *stale = view_cast<DockSplitter>(child.get())) {
			only->remove_child(stale);
			break;
		}
	}
	for (const auto &child : container->get_children()) {
		if (auto *splitter = view_cast<DockSplitter>(child.get())) {
			only->add_child(container->detach_child(splitter));
			break;
		}
	}

	if (container == m_root) {
		replace_child(container, std::move(owned));
		m_root = only;
	} else if (auto *gp_node = view_cast<DockNode>(grandparent)) {
		gp_node->replace_child(container, std::move(owned));
	} else if (grandparent != nullptr) {
		grandparent->replace_child(container, std::move(owned));
	}

	if (grandparent != nullptr) {
		grandparent->invalidate_layout();
	}
}

bool DockSpace::is_outside_canvas(Vec2 pos) const {
	const Canvas *canvas = get_canvas();
	if (canvas == nullptr) {
		return false;
	}
	const auto w = static_cast<float>(canvas->get_width());
	const auto h = static_cast<float>(canvas->get_height());
	return pos.x < 0.F || pos.y < 0.F || pos.x >= w || pos.y >= h;
}

void DockSpace::preview_external_drag(Vec2 local_pos) {
	DockNode *hovered = m_root->hit_test_node(local_pos);
	if (hovered != m_drop_target) {
		if (m_drop_target != nullptr) {
			m_drop_target->show_drop_zones(false);
			m_drop_target->highlight_drop_zone(DropZone::None);
		}
		update_preview(nullptr, DropZone::None);
		m_drop_target = hovered;
		if (m_drop_target != nullptr) {
			m_drop_target->show_drop_zones(true);
		}
		m_current_zone = DropZone::None;
	}

	if (m_drop_target == nullptr) {
		return;
	}

	DropZone zone = m_drop_target->hit_test_drop_zone(local_pos);
	if (zone != m_current_zone) {
		m_drop_target->highlight_drop_zone(zone);
		m_current_zone = zone;
		update_preview(m_drop_target, zone);
	}
}

void DockSpace::clear_external_drag() {
	if (m_drop_target != nullptr) {
		m_drop_target->show_drop_zones(false);
		m_drop_target->highlight_drop_zone(DropZone::None);
	}
	update_preview(nullptr, DropZone::None);
	m_drop_target = nullptr;
	m_current_zone = DropZone::None;
}

void DockSpace::begin_external_drag(DockPanel *panel, const std::string &title) {
	m_drag_ctx.active = true;
	m_drag_ctx.source_node = nullptr;
	m_drag_ctx.panel = panel;
	m_drag_ctx.title = title;
	m_drag_ctx.external_view.reset();
	m_drop_target = nullptr;
	m_current_zone = DropZone::None;
}

namespace {

struct HarvestedPanel {
	Unique<View> view;
	std::string title;
};

void harvest_panels(DockNode *node, std::unordered_map<std::string, HarvestedPanel> &by_id,
					std::vector<HarvestedPanel> &extras) {
	if (node == nullptr) {
		return;
	}
	if (node->is_leaf()) {
		for (DockPanel *panel : node->get_ordered_panels()) {
			if (panel == nullptr) {
				continue;
			}
			std::string id = panel->get_id();
			std::string title = panel->get_title();
			Unique<View> owned = node->detach_panel(panel);
			if (!owned) {
				continue;
			}
			if (!id.empty() && by_id.find(id) == by_id.end()) {
				by_id.emplace(std::move(id), HarvestedPanel{ std::move(owned), std::move(title) });
			} else {
				extras.push_back(HarvestedPanel{ std::move(owned), std::move(title) });
			}
		}
		return;
	}
	for (const auto &child : node->get_children()) {
		if (DockNode *dn = view_cast<DockNode>(child.get())) {
			harvest_panels(dn, by_id, extras);
		}
	}
}

DockNode *find_first_leaf(DockNode *node) {
	if (node == nullptr) {
		return nullptr;
	}
	if (node->is_leaf()) {
		return node;
	}
	for (const auto &child : node->get_children()) {
		if (DockNode *dn = view_cast<DockNode>(child.get())) {
			if (DockNode *leaf = find_first_leaf(dn)) {
				return leaf;
			}
		}
	}
	return nullptr;
}

void apply_child_fractions(const std::vector<DockNode *> &leaves, const std::vector<DockNodeDesc> &children,
						   SplitDirection dir) {
	constexpr std::size_t k_grower = 1;
	for (std::size_t i = 0; i < leaves.size(); ++i) {
		StyleProperties sp;
		if (i == k_grower) {
			sp.flex_grow = 1.F;
		} else {
			sp.flex_grow = 0.F;
			const F32 fraction = (i < children.size()) ? children[i].fraction : 0.F;
			const F32 pct = Math::clamp(fraction * 100.F, 1.F, 99.F);
			if (dir == SplitDirection::Horizontal) {
				sp.width = StyleLength::percent(pct);
			} else {
				sp.height = StyleLength::percent(pct);
			}
		}
		leaves[i]->merge_style(sp);
	}
}

void realize_desc(DockNode *node, const DockNodeDesc &desc, std::unordered_map<std::string, HarvestedPanel> &by_id,
				  const DockSpace::PanelFactory &factory) {
	if (node == nullptr) {
		return;
	}

	if (desc.is_leaf) {
		for (const std::string &id : desc.panel_ids) {
			auto it = by_id.find(id);
			if (it != by_id.end() && it->second.view) {
				node->accept_panel(std::move(it->second.view), it->second.title, DropZone::Center);
				by_id.erase(it);
				continue;
			}
			if (!factory) {
				continue;
			}
			if (Unique<DockPanel> created = factory(id)) {
				std::string title = created->get_title();
				node->accept_panel(std::move(created), std::move(title), DropZone::Center);
			}
		}
		const int count = node->get_tab_count();
		if (count > 0) {
			int active = desc.active_index;
			if (active < 0 || active >= count) {
				active = 0;
			}
			node->set_active_panel(active);
		}
		return;
	}

	if (desc.children.empty()) {
		return;
	}
	if (desc.children.size() == 1) {
		realize_desc(node, desc.children[0], by_id, factory);
		return;
	}

	const SplitDirection dir = (desc.direction == 1) ? SplitDirection::Vertical : SplitDirection::Horizontal;

	std::vector<DockNode *> leaves;
	auto [first, second] = node->split(dir);
	leaves.push_back(first);
	leaves.push_back(second);
	for (std::size_t i = 2; i < desc.children.size(); ++i) {
		DockNode *leaf = node->append_leaf(dir);
		if (leaf == nullptr) {
			break;
		}
		leaves.push_back(leaf);
	}

	apply_child_fractions(leaves, desc.children, dir);

	for (std::size_t i = 0; i < leaves.size() && i < desc.children.size(); ++i) {
		realize_desc(leaves[i], desc.children[i], by_id, factory);
	}
}

} // namespace

bool DockSpace::apply_layout(const DockLayoutDesc &desc, bool discard_unplaced) {
	if (m_root == nullptr) {
		return false;
	}

	std::unordered_map<std::string, HarvestedPanel> by_id;
	std::vector<HarvestedPanel> extras;
	harvest_panels(m_root, by_id, extras);

	auto fresh = std::make_unique<DockNode>(&m_drag_ctx);
	m_root = dynamic_cast<DockNode *>(replace_child(m_root, std::move(fresh)));

	realize_desc(m_root, desc.root, by_id, m_panel_factory);

	DockNode *fallback = find_first_leaf(m_root);
	if (fallback != nullptr && !discard_unplaced) {
		for (auto &entry : by_id) {
			if (entry.second.view) {
				fallback->accept_panel(std::move(entry.second.view), entry.second.title, DropZone::Center);
			}
		}
		for (auto &entry : extras) {
			if (entry.view) {
				fallback->accept_panel(std::move(entry.view), entry.title, DropZone::Center);
			}
		}
	}

	invalidate_layout();
	return true;
}

void DockSpace::set_tab_bar_decorator(std::function<void(DockNode *, View *)> decorator) {
	m_drag_ctx.decorate_tab_bar = std::move(decorator);
	for_each_leaf([](DockNode *leaf) { leaf->decorate_tab_bar(); });
}

void DockSpace::set_closable_tabs(bool closable, GFX::GfxTexture *close_icon) {
	m_drag_ctx.closable_tabs = closable;
	m_drag_ctx.close_icon = close_icon;
}

namespace {

void visit_leaves(DockNode *node, const std::function<void(DockNode *)> &visit) {
	if (node == nullptr) {
		return;
	}
	if (node->is_leaf()) {
		visit(node);
		return;
	}
	for (const auto &child : node->get_children()) {
		if (auto *dn = view_cast<DockNode>(child.get())) {
			visit_leaves(dn, visit);
		}
	}
}

} // namespace

void DockSpace::for_each_leaf(const std::function<void(DockNode *)> &visit) const {
	visit_leaves(m_root, visit);
}

DockNode *DockSpace::find_node_of(const DockPanel *panel) const {
	DockNode *found = nullptr;
	for_each_leaf([&](DockNode *leaf) {
		if (found == nullptr && leaf->index_of(panel) >= 0) {
			found = leaf;
		}
	});
	return found;
}

bool DockSpace::move_panel(DockPanel *panel, DockNode *target, DropZone zone) {
	DockNode *source = find_node_of(panel);
	if (source == nullptr || target == nullptr || (source == target && zone == DropZone::Center)) {
		return false;
	}
	std::string title = panel->get_title();
	Unique<View> owned = source->detach_panel(panel);
	if (!owned) {
		return false;
	}
	if (source != target && source->is_empty()) {
		collapse_node(source);
	}
	target->accept_panel(std::move(owned), std::move(title), zone);
	return true;
}

void DockSpace::close_panel(DockPanel *panel) {
	if (DockNode *node = find_node_of(panel)) {
		node->close_panel(panel);
	}
}

bool DockSpace::try_dock_external(Unique<View> &panel_view, const std::string &title, Vec2 local_pos) {
	preview_external_drag(local_pos);

	DockNode *target = m_drop_target;
	DropZone zone = m_current_zone;

	const bool can_dock = (target != nullptr && zone != DropZone::None);
	if (can_dock) {
		// resolves it — no direct AcceptPanel from the external path.
		if (!title.empty()) {
			m_drag_ctx.title = title;
		}
		m_drag_ctx.external_view = std::move(panel_view);
		execute_drop(target, zone, local_pos);
	}

	clear_external_drag();
	m_drag_ctx.active = false;
	m_drag_ctx.source_node = nullptr;
	m_drag_ctx.panel = nullptr;
	m_drag_ctx.title.clear();
	m_drag_ctx.external_view.reset();
	return can_dock;
}

} // namespace Aquila::UI::Core
