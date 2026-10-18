#include "Aquila/UI/DevTools/UIDebugWindow.h"

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/Platform/Events/Event.h"
#include "Aquila/Platform/Events/WindowEvent.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Style/ComputedStyle.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Style/StyleProperties.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/ScrollView.h"
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

Label *make_label(View *parent, const std::string &text, const char *cls) {
	auto *label = parent->add_child<Label>(text);
	label->add_class(cls);
	return label;
}

View *make_view(View *parent, const char *cls) {
	auto *view = parent->add_child<View>();
	view->add_class(cls);
	return view;
}

} // namespace

UIDebugWindow::UIDebugWindow() = default;
UIDebugWindow::~UIDebugWindow() = default;

void UIDebugWindow::build(Canvas *target, Uint32 width, Uint32 height, const std::vector<std::string> &style_paths) {
	m_target = target;
	m_canvas = std::make_unique<Canvas>(width, height);
	UI::StyleParser::load_files(style_paths, m_canvas->get_style_sheet());

	auto *root = m_canvas->get_root();
	root->set_id("inspector-root");
	root->add_class("inspector-root");

	build_toolbar(root);

	auto *body = make_view(root, "inspector-body");
	build_tree_pane(body);
	build_style_pane(body);

	auto *footer = make_view(root, "inspector-breadcrumb-bar");
	m_breadcrumb = make_label(footer, "", "inspector-breadcrumb");

	m_canvas->reload_styles();
	refresh();
	populate(nullptr);
}

void UIDebugWindow::build_toolbar(View *parent) {
	auto *bar = make_view(parent, "inspector-toolbar");

	auto *inspect = bar->add_child<Button>(std::string("Inspect"));
	inspect->add_class("inspector-tool-btn");
	inspect->on_click.connect([this] {
		if (on_pick_requested) {
			on_pick_requested();
		}
	});

	auto *refresh = bar->add_child<Button>(std::string("Refresh"));
	refresh->add_class("inspector-tool-btn");
	refresh->on_click.connect([this] { this->refresh(); });

	make_view(bar, "inspector-tool-spacer");

	m_search = bar->add_child<TextInput>(std::string("Filter styles"));
	m_search->add_class("inspector-search");
	m_search->on_changed.connect([this](const std::string &text) {
		m_filter = Foundation::to_lower(text);
		apply_filter();
	});
}

void UIDebugWindow::build_tree_pane(View *parent) {
	auto *scroll = parent->add_child<ScrollView>();
	scroll->add_class("inspector-tree-scroll");
	m_tree_host = make_view(scroll, "inspector-tree-host");
}

void UIDebugWindow::build_style_pane(View *parent) {
	auto *scroll = parent->add_child<ScrollView>();
	scroll->add_class("inspector-style-scroll");
	auto *host = make_view(scroll, "inspector-style");

	m_empty_hint = make_view(host, "inspector-empty");
	make_label(m_empty_hint, "Press Inspect, hover the editor to highlight, then click an element to inspect it.",
			   "inspector-empty-label");

	m_detail_body = make_view(host, "inspector-detail");
	m_subtitle = make_label(m_detail_body, "", "inspector-subtitle");

	build_box_model(m_detail_body);

	auto *element = begin_section(m_detail_body, "Element");
	m_v_type = add_row(element, "type");
	m_v_id = add_row(element, "id");
	m_v_classes = add_row(element, "classes");
	m_v_state = add_row(element, "state");
	m_v_children = add_row(element, "children");

	auto *geometry = begin_section(m_detail_body, "Geometry");
	m_v_pos = add_row(geometry, "position");
	m_v_size = add_row(geometry, "size");
	m_v_ids = add_row(geometry, "ids");

	auto *layout = begin_section(m_detail_body, "Layout");
	m_v_display = add_row(layout, "display");
	m_v_position = add_row(layout, "position");
	m_v_overflow = add_row(layout, "overflow");
	m_v_flex = add_row(layout, "flex-direction");
	m_v_justify = add_row(layout, "justify");
	m_v_align = add_row(layout, "align");
	m_v_grow = add_row(layout, "flex-grow");
	m_v_gap = add_row(layout, "gap");
	m_v_dims = add_row(layout, "width / height");
	m_v_minmax = add_row(layout, "min / max");
	m_v_aspect = add_row(layout, "aspect-ratio");
	m_v_inset = add_row(layout, "inset");
	m_v_zindex = add_row(layout, "z-index");

	auto *appearance = begin_section(m_detail_body, "Appearance");
	m_v_bg = add_color_row(appearance, "background", &m_sw_bg);
	m_v_color = add_color_row(appearance, "color", &m_sw_color);
	m_v_border = add_color_row(appearance, "border", &m_sw_border);
	m_v_radius = add_row(appearance, "border-radius");
	m_v_opacity = add_row(appearance, "opacity");
	m_v_shadows = add_row(appearance, "box-shadows");

	auto *text = begin_section(m_detail_body, "Text");
	m_v_fontsize = add_row(text, "font-size");
	m_v_family = add_row(text, "font-family");
	m_v_textalign = add_row(text, "text-align");
	m_v_whitespace = add_row(text, "white-space");
}

void UIDebugWindow::build_box_model(View *parent) {
	auto *section = make_view(parent, "inspector-section");
	make_label(section, "Box Model", "inspector-section-header");

	auto *wrap = make_view(section, "bm-wrap");
	auto *padbox = make_view(wrap, "bm-padbox");
	m_box_pad_top = make_label(padbox, "", "bm-pad");

	auto *mid = make_view(padbox, "bm-mid");
	m_box_pad_left = make_label(mid, "", "bm-pad");
	auto *content = make_view(mid, "bm-content");
	m_box_content = make_label(content, "", "bm-content-label");
	m_box_pad_right = make_label(mid, "", "bm-pad");

	m_box_pad_bottom = make_label(padbox, "", "bm-pad");
	make_label(wrap, "padding", "bm-caption");
}

View *UIDebugWindow::begin_section(View *parent, const std::string &title) {
	auto *section = make_view(parent, "inspector-section");
	make_label(section, title, "inspector-section-header");
	m_sections.push_back(section);
	return section;
}

Label *UIDebugWindow::add_row(View *section, const std::string &key) {
	auto *row = make_view(section, "inspector-row");
	make_label(row, key, "inspector-key");
	auto *value = make_label(row, "", "inspector-val");
	m_rows.push_back({ row, value, section, Foundation::to_lower(key) });
	return value;
}

Label *UIDebugWindow::add_color_row(View *section, const std::string &key, View **out_swatch) {
	auto *row = make_view(section, "inspector-row");
	make_label(row, key, "inspector-key");
	*out_swatch = make_view(row, "inspector-swatch");
	auto *value = make_label(row, "", "inspector-val");
	m_rows.push_back({ row, value, section, Foundation::to_lower(key) });
	return value;
}

void UIDebugWindow::set_swatch(View *swatch, Vec4 color) {
	if (swatch == nullptr) {
		return;
	}
	StyleProperties props;
	props.background_color = color;
	swatch->merge_style(props);
}

void UIDebugWindow::update(F32 delta_time) {
	m_canvas->update(delta_time);
	m_canvas->compute();
}

void UIDebugWindow::render(Graphics::Renderer2D &batcher, GFX::GfxCommandList &cmd) {
	m_canvas->submit_to_quad_batcher(batcher, cmd);
}

void UIDebugWindow::set_frame_target(const void *target) {
	m_canvas->set_frame_target(target);
}

void UIDebugWindow::on_event(Platform::Events::Event &event) {
	Platform::Events::EventDispatcher dispatcher(event);
	dispatcher.dispatch<Platform::Events::WindowResizeEvent>([this](Platform::Events::WindowResizeEvent &e) {
		if (e.get_width() > 0 && e.get_height() > 0) {
			m_canvas->resize(e.get_width(), e.get_height());
		}
		return false;
	});

	m_canvas->on_event(event);
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
	if ((m_target == nullptr) || (m_tree_host == nullptr)) {
		return;
	}

	m_node_to_view.clear();
	m_view_to_node.clear();
	while (!m_tree_host->get_children().empty()) {
		m_tree_host->remove_child(m_tree_host->get_children().front().get());
	}

	m_tree = m_tree_host->add_child<TreeView>();
	m_tree->on_selected.connect([this](TreeNode *node) {
		auto it = m_node_to_view.find(node);
		View *view = it != m_node_to_view.end() ? it->second : nullptr;
		populate(view);
		if (view && on_view_highlighted) {
			on_view_highlighted(view);
		}
	});

	for (const auto &child : m_target->get_root()->get_children()) {
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
	if (it == m_view_to_node.end()) {
		return;
	}

	for (View *parent = view->get_parent(); parent != nullptr; parent = parent->get_parent()) {
		auto parent_it = m_view_to_node.find(parent);
		if (parent_it != m_view_to_node.end()) {
			parent_it->second->set_expanded(true);
		}
	}
	m_tree->select_node(it->second);
	m_canvas->scroll_into_view(it->second);
}

void UIDebugWindow::populate(View *view) {
	m_current = view;

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

	m_subtitle->set_text(short_label(view));

	m_v_type->set_text(std::string(view->get_type_name()));
	m_v_id->set_text(view->get_id().empty() ? "—" : view->get_id());
	const std::string classes = join_classes(view);
	m_v_classes->set_text(classes.empty() ? "—" : classes);
	m_v_state->set_text(state_str(view));
	m_v_children->set_text(std::to_string(view->get_children().size()));

	m_v_pos->set_text(num(rect.position.x) + ", " + num(rect.position.y));
	m_v_size->set_text(num(rect.size.x) + " × " + num(rect.size.y));
	m_v_ids->set_text("clay " + std::to_string(view->get_clay_id()) + "   stable " +
					  std::to_string(view->get_stable_id()) + "   z " + std::to_string(view->get_effective_z()));

	m_v_display->set_text(str(cs.display));
	m_v_position->set_text(str(cs.position));
	m_v_overflow->set_text(str(cs.overflow));
	m_v_flex->set_text(std::string(str(cs.flex_direction)) + "  ·  " + str(cs.wrap));
	m_v_justify->set_text(str(cs.justify));
	m_v_align->set_text(str(cs.align));
	m_v_grow->set_text(num(cs.flex_grow));
	m_v_gap->set_text(num(cs.gap));
	m_v_dims->set_text(len_str(cs.width) + " × " + len_str(cs.height));
	m_v_minmax->set_text(len_str(cs.min_width) + " × " + len_str(cs.min_height) + "   /   " + len_str(cs.max_width) +
						 " × " + len_str(cs.max_height));
	m_v_aspect->set_text(cs.aspect_ratio > 0.F ? num(cs.aspect_ratio) : "—");
	m_v_inset->set_text(len_str(cs.top) + " " + len_str(cs.right) + " " + len_str(cs.bottom) + " " + len_str(cs.left));
	m_v_zindex->set_text(std::to_string(cs.z_index));

	m_v_bg->set_text(color_str(cs.background_color));
	set_swatch(m_sw_bg, cs.background_color);
	m_v_color->set_text(color_str(cs.color));
	set_swatch(m_sw_color, cs.color);
	m_v_border->set_text(num(cs.border_width) + "px  ·  " + str(cs.border_style) + "  ·  " + color_str(cs.border_color));
	set_swatch(m_sw_border, cs.border_color);
	m_v_radius->set_text(num(cs.border_radius.x) + " " + num(cs.border_radius.y) + " " + num(cs.border_radius.z) + " " +
						 num(cs.border_radius.w));
	m_v_opacity->set_text(num(cs.opacity));
	m_v_shadows->set_text(std::to_string(cs.box_shadows.size()));

	m_v_fontsize->set_text(cs.font_size > 0.F ? num(cs.font_size) : "inherit");
	m_v_family->set_text(cs.font_family.empty() ? "inherit" : cs.font_family);
	m_v_textalign->set_text(str(cs.text_align));
	m_v_whitespace->set_text(str(cs.white_space));

	m_box_pad_top->set_text(len_str(cs.padding.top));
	m_box_pad_right->set_text(len_str(cs.padding.right));
	m_box_pad_bottom->set_text(len_str(cs.padding.bottom));
	m_box_pad_left->set_text(len_str(cs.padding.left));
	m_box_content->set_text(num(rect.size.x) + " × " + num(rect.size.y));

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
