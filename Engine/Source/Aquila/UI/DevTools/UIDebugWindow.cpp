#include "Aquila/UI/DevTools/UIDebugWindow.h"

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/UIResources.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Style/ComputedStyle.h"
#include "Aquila/UI/Style/StyleProperties.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Aquila/UI/Widgets/TreeView.h"

#include <cstdio>

namespace Aquila::UI::DevTools {

using namespace Aquila::UI::Core;

namespace {

using namespace Aquila::UI;

std::string join_classes(View *view) {
	std::string out;
	for (const auto &cls : view->get_classes()) {
		out += (out.empty() ? "" : " ") + cls;
	}
	return out;
}

std::string short_label(View *view) {
	std::string label(view->get_type_name());
	if (!view->get_id().empty()) {
		label += " #" + view->get_id();
	}
	const auto &classes = view->get_classes();
	if (!classes.empty()) {
		label += " ." + classes.front();
	}
	return label;
}

std::string full_label(View *view) {
	std::string label(view->get_type_name());
	if (!view->get_id().empty()) {
		label += " #" + view->get_id();
	}
	for (const auto &cls : view->get_classes()) {
		label += " ." + cls;
	}
	return label;
}

std::string state_str(View *view) {
	std::string out;
	auto add = [&](const std::string &token) { out += (out.empty() ? "" : " · ") + token; };
	add(view->is_visible() ? "visible" : "hidden");
	if (!view->is_enabled()) {
		add("disabled");
	}
	if (view->is_hovered()) {
		add("hover");
	}
	if (view->is_pressed()) {
		add("press");
	}
	if (view->is_focused()) {
		add("focus");
	}
	return out;
}

const char *str(Display d) {
	return d == Display::Flex ? "flex" : "none";
}
const char *str(Overflow o) {
	switch (o) {
	case Overflow::Visible:
		return "visible";
	case Overflow::Hidden:
		return "hidden";
	case Overflow::Scroll:
		return "scroll";
	case Overflow::Clip:
		return "clip";
	}
	return "visible";
}
const char *str(Position p) {
	switch (p) {
	case Position::Relative:
		return "relative";
	case Position::Absolute:
		return "absolute";
	default:
		return "static";
	}
}
const char *str(FlexDirection f) {
	switch (f) {
	case FlexDirection::Column:
		return "column";
	case FlexDirection::RowReverse:
		return "row-reverse";
	case FlexDirection::ColumnReverse:
		return "column-reverse";
	default:
		return "row";
	}
}
const char *str(FlexWrap w) {
	return w == FlexWrap::Wrap ? "wrap" : "nowrap";
}
const char *str(JustifyContent j) {
	switch (j) {
	case JustifyContent::End:
		return "end";
	case JustifyContent::Center:
		return "center";
	default:
		return "start";
	}
}
const char *str(AlignItems a) {
	switch (a) {
	case AlignItems::End:
		return "end";
	case AlignItems::Center:
		return "center";
	case AlignItems::Stretch:
		return "stretch";
	default:
		return "start";
	}
}
const char *str(TextAlign t) {
	return t == TextAlign::Center ? "center" : (t == TextAlign::Right ? "right" : "left");
}
const char *str(WhiteSpace w) {
	return w == WhiteSpace::Nowrap ? "nowrap" : "normal";
}
const char *str(BorderStyle b) {
	return b == BorderStyle::Dashed ? "dashed" : (b == BorderStyle::Dotted ? "dotted" : "solid");
}

std::string num(F32 v) {
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%g", v);
	return buf;
}

std::string len_str(StyleLength l) {
	switch (l.unit) {
	case LengthUnit::Pixel:
		return num(l.value);
	case LengthUnit::Percent:
		return num(l.value) + "%";
	case LengthUnit::Grow:
		return "grow";
	default:
		return "auto";
	}
}

std::string color_str(Vec4 c) {
	auto ch = [](float v) { return static_cast<int>(Math::clamp(v, 0.F, 1.F) * 255.F + 0.5F); };
	char buf[40];
	std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", ch(c.r), ch(c.g), ch(c.b), ch(c.a));
	return buf;
}

constexpr std::array<std::string_view, 5> K_SECTIONS = { "section-element", "section-geometry", "section-layout",
														 "section-appearance", "section-text" };

constexpr std::array<std::string_view, 37> K_VALUES = {
	"v-type",
	"v-id",
	"v-classes",
	"v-state",
	"v-children",
	"v-pos",
	"v-size",
	"v-ids",
	"v-display",
	"v-position",
	"v-overflow",
	"v-flex",
	"v-justify",
	"v-align",
	"v-grow",
	"v-gap",
	"v-dims",
	"v-minmax",
	"v-aspect",
	"v-inset",
	"v-zindex",
	"v-bg",
	"v-color",
	"v-border",
	"v-radius",
	"v-opacity",
	"v-shadows",
	"v-fontsize",
	"v-family",
	"v-textalign",
	"v-whitespace",
	"bm-top",
	"bm-right",
	"bm-bottom",
	"bm-left",
	"bm-content",
	"inspector-subtitle",
};

} // namespace

UIDebugWindow::UIDebugWindow(Canvas &target)
	: WindowBehaviour(std::string(UI_DIR) + "Windows/ui_debug.aqlayout"), m_target(target) {}

UIDebugWindow::~UIDebugWindow() = default;

void UIDebugWindow::define_behaviour() {
	if (auto *inspect = get<Button>("inspector-pick")) {
		inspect->on_click.connect([this] {
			if (on_pick_requested) {
				on_pick_requested();
			}
		});
	}
	if (auto *refresh_button = get<Button>("inspector-refresh")) {
		refresh_button->on_click.connect([this] { refresh(); });
	}
	if (auto *search = get<TextInput>("inspector-filter")) {
		search->on_changed.connect([this](const std::string &text) {
			m_filter = Foundation::to_lower(text);
			apply_filter();
		});
	}

	m_tree = get<TreeView>("inspector-tree");
	if (m_tree != nullptr) {
		m_tree->on_selected.connect([this](TreeNode *node) {
			auto it = m_node_to_view.find(node);
			View *view = it != m_node_to_view.end() ? it->second : nullptr;
			populate(view);
			if (view && on_view_highlighted) {
				on_view_highlighted(view);
			}
		});
	}

	m_empty_hint = get("inspector-empty");
	m_detail_body = get("inspector-detail");
	m_breadcrumb = get<Label>("inspector-breadcrumb");
	for (const std::string_view id : K_VALUES) {
		if (auto *value = get<Label>(id)) {
			m_values.emplace(std::string(id), value);
		}
	}
	m_sw_bg = get("sw-bg");
	m_sw_color = get("sw-color");
	m_sw_border = get("sw-border");
	collect_rows();

	refresh();
	populate(nullptr);
}

void UIDebugWindow::collect_rows() {
	for (const std::string_view id : K_SECTIONS) {
		View *section = get(id);
		if (section == nullptr) {
			continue;
		}
		m_sections.push_back(section);
		const auto &children = section->get_children();
		for (Usize i = 1; i < children.size(); ++i) {
			View *row = children[i].get();
			const auto &parts = row->get_children();
			const auto *key = parts.empty() ? nullptr : dynamic_cast<const Label *>(parts.front().get());
			auto *value = parts.empty() ? nullptr : dynamic_cast<Label *>(parts.back().get());
			if (key != nullptr && value != nullptr) {
				m_rows.push_back({ row, value, section, Foundation::to_lower(key->get_text()) });
			}
		}
	}
}

void UIDebugWindow::set_value(std::string_view id, const std::string &text) {
	if (const auto it = m_values.find(std::string(id)); it != m_values.end()) {
		it->second->set_text(text);
	}
}

void UIDebugWindow::set_swatch(View *swatch, Vec4 color) {
	if (swatch == nullptr) {
		return;
	}
	StyleProperties props;
	props.background_color = color;
	swatch->merge_style(props);
}

TreeNode *UIDebugWindow::add_view_node(View *view, TreeNode *parent_node) {
	TreeNode *node = (parent_node != nullptr) ? parent_node->add_child_node(full_label(view)) : m_tree->add_node(full_label(view));
	m_node_to_view[node] = view;
	m_view_to_node[view] = node;

	for (const auto &child : view->get_children()) {
		if (child.get() == m_ignored) {
			continue;
		}
		add_view_node(child.get(), node);
	}
	if (parent_node != nullptr) {
		node->set_expanded(false);
	}
	return node;
}

void UIDebugWindow::refresh() {
	if (m_tree == nullptr) {
		return;
	}

	m_node_to_view.clear();
	m_view_to_node.clear();
	m_tree->clear();

	for (const auto &child : m_target.get_root()->get_children()) {
		if (child.get() == m_ignored) {
			continue;
		}
		add_view_node(child.get(), nullptr);
	}
}

void UIDebugWindow::select_view(View *view) {
	populate(view);
	if (view == nullptr) {
		return;
	}

	auto it = m_view_to_node.find(view);
	if (it == m_view_to_node.end() || m_tree == nullptr) {
		return;
	}

	for (View *parent = view->get_parent(); parent != nullptr; parent = parent->get_parent()) {
		auto parent_it = m_view_to_node.find(parent);
		if (parent_it != m_view_to_node.end()) {
			parent_it->second->set_expanded(true);
		}
	}
	m_tree->select_node(it->second);
	if (Canvas *canvas = root().get_canvas()) {
		canvas->scroll_into_view(it->second);
	}
}

void UIDebugWindow::populate(View *view) {
	m_current = view;
	if (m_empty_hint == nullptr || m_detail_body == nullptr || m_breadcrumb == nullptr) {
		return;
	}

	if (view == nullptr) {
		m_empty_hint->set_hidden(false);
		m_detail_body->set_hidden(true);
		m_breadcrumb->set_text("");
		return;
	}

	m_empty_hint->set_hidden(true);
	m_detail_body->set_hidden(false);

	const UI::ComputedStyle &cs = view->get_computed_style();
	const Rect rect = view->get_absolute_rect();

	set_value("inspector-subtitle", short_label(view));

	set_value("v-type", std::string(view->get_type_name()));
	set_value("v-id", view->get_id().empty() ? "—" : view->get_id());
	const std::string classes = join_classes(view);
	set_value("v-classes", classes.empty() ? "—" : classes);
	set_value("v-state", state_str(view));
	set_value("v-children", std::to_string(view->get_children().size()));

	set_value("v-pos", num(rect.position.x) + ", " + num(rect.position.y));
	set_value("v-size", num(rect.size.x) + " × " + num(rect.size.y));
	set_value("v-ids",
			  "clay " + std::to_string(view->get_clay_id()) + "   stable " + std::to_string(view->get_stable_id()) +
				  "   z " + std::to_string(view->get_effective_z()));

	set_value("v-display", str(cs.display));
	set_value("v-position", str(cs.position));
	set_value("v-overflow", str(cs.overflow));
	set_value("v-flex", std::string(str(cs.flex_direction)) + "  ·  " + str(cs.wrap));
	set_value("v-justify", str(cs.justify));
	set_value("v-align", str(cs.align));
	set_value("v-grow", num(cs.flex_grow));
	set_value("v-gap", num(cs.gap));
	set_value("v-dims", len_str(cs.width) + " × " + len_str(cs.height));
	set_value("v-minmax",
			  len_str(cs.min_width) + " × " + len_str(cs.min_height) + "   /   " + len_str(cs.max_width) + " × " +
				  len_str(cs.max_height));
	set_value("v-aspect", cs.aspect_ratio > 0.F ? num(cs.aspect_ratio) : "—");
	set_value("v-inset", len_str(cs.top) + " " + len_str(cs.right) + " " + len_str(cs.bottom) + " " + len_str(cs.left));
	set_value("v-zindex", std::to_string(cs.z_index));

	set_value("v-bg", color_str(cs.background_color));
	set_swatch(m_sw_bg, cs.background_color);
	set_value("v-color", color_str(cs.color));
	set_swatch(m_sw_color, cs.color);
	set_value("v-border",
			  num(cs.border_width) + "px  ·  " + str(cs.border_style) + "  ·  " + color_str(cs.border_color));
	set_swatch(m_sw_border, cs.border_color);
	set_value("v-radius",
			  num(cs.border_radius.x) + " " + num(cs.border_radius.y) + " " + num(cs.border_radius.z) + " " +
				  num(cs.border_radius.w));
	set_value("v-opacity", num(cs.opacity));
	set_value("v-shadows", std::to_string(cs.box_shadows.size()));

	set_value("v-fontsize", cs.font_size > 0.F ? num(cs.font_size) : "inherit");
	set_value("v-family", cs.font_family.empty() ? "inherit" : cs.font_family);
	set_value("v-textalign", str(cs.text_align));
	set_value("v-whitespace", str(cs.white_space));

	set_value("bm-top", len_str(cs.padding.top));
	set_value("bm-right", len_str(cs.padding.right));
	set_value("bm-bottom", len_str(cs.padding.bottom));
	set_value("bm-left", len_str(cs.padding.left));
	set_value("bm-content", num(rect.size.x) + " × " + num(rect.size.y));

	std::string trail;
	std::vector<std::string> chain;
	for (View *node = view; node != nullptr; node = node->get_parent()) {
		chain.push_back(short_label(node));
	}
	for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
		trail += (trail.empty() ? "" : "  ›  ") + *it;
	}
	m_breadcrumb->set_text(trail);

	apply_filter();
}

void UIDebugWindow::apply_filter() {
	std::unordered_map<View *, int> visible;
	for (View *section : m_sections) {
		visible[section] = 0;
	}

	for (const PropRow &prop : m_rows) {
		const bool match = m_filter.empty() || prop.key.find(m_filter) != std::string::npos ||
						   Foundation::to_lower(prop.value->get_text()).find(m_filter) != std::string::npos;
		prop.row->set_hidden(!match);
		if (match) {
			visible[prop.section]++;
		}
	}

	for (View *section : m_sections) {
		section->set_hidden(visible[section] == 0);
	}
}

} // namespace Aquila::UI::DevTools
