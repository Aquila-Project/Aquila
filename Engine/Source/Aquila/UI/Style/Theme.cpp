#include "Aquila/UI/Style/Theme.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/UI/Style/StyleSheet.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include <cstdio>

namespace Aquila::UI {

void Theme::set(std::string_view type_name, StyleProperties props) {
	m_styles[{ std::string(type_name), "" }] = std::move(props);
}

void Theme::set(std::string_view type_name, std::string_view pseudo_class, StyleProperties props) {
	m_styles[{ std::string(type_name), std::string(pseudo_class) }] = std::move(props);
}

void Theme::set_color(std::string_view name, Vec4 color) {
	m_colors[std::string(name)] = color;
}

void Theme::set_constant(std::string_view name, float value) {
	m_constants[std::string(name)] = value;
}

Option<Vec4> Theme::get_color(std::string_view name) const {
	const auto it = m_colors.find(std::string(name));
	return it != m_colors.end() ? Option<Vec4>(it->second) : std::nullopt;
}

Option<float> Theme::get_constant(std::string_view name) const {
	const auto it = m_constants.find(std::string(name));
	return it != m_constants.end() ? Option<float>(it->second) : std::nullopt;
}

const StyleProperties *Theme::get(std::string_view type_name, std::string_view pseudo_class) const {
	const auto it = m_styles.find({ std::string(type_name), std::string(pseudo_class) });
	return it != m_styles.end() ? &it->second : nullptr;
}

void Theme::apply_to_style_sheet(StyleSheet &sheet) const {
	for (const auto &[key, props] : m_styles) {
		const StyleRule::SelectorType type = StyleRule::SelectorType::Type;
		sheet.add_rule(type, key.type_name, key.pseudo_class, props);
	}
}

static std::string fmt_float(float v) {
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%.4g", v);
	return buf;
}

static std::string fmt_color(const Vec4 &c) {
	char buf[64];
	std::snprintf(buf, sizeof(buf), "rgba(%.3f, %.3f, %.3f, %.3f)", Math::clamp(c.r, 0.F, 1.F),
				  Math::clamp(c.g, 0.F, 1.F), Math::clamp(c.b, 0.F, 1.F), Math::clamp(c.a, 0.F, 1.F));
	return buf;
}

static std::string fmt_length(const StyleLength &l) {
	char buf[32];
	switch (l.unit) {
	case LengthUnit::Auto:
		return "auto";
	case LengthUnit::Grow:
		return "grow";
	case LengthUnit::Percent:
		std::snprintf(buf, sizeof(buf), "%.4g%%", l.value);
		return buf;
	case LengthUnit::Vw:
		std::snprintf(buf, sizeof(buf), "%.4gvw", l.value);
		return buf;
	case LengthUnit::Vh:
		std::snprintf(buf, sizeof(buf), "%.4gvh", l.value);
		return buf;
	default:
		std::snprintf(buf, sizeof(buf), "%.4gpx", l.value);
		return buf;
	}
}

static std::string fmt_edges(const StyleEdges &e) {
	if (e.top == e.right && e.right == e.bottom && e.bottom == e.left) {
		return fmt_length(e.top);
	}
	if (e.top == e.bottom && e.left == e.right) {
		return fmt_length(e.top) + " " + fmt_length(e.right);
	}
	return fmt_length(e.top) + " " + fmt_length(e.right) + " " + fmt_length(e.bottom) + " " + fmt_length(e.left);
}

static void decl(std::string &out, std::string_view prop, std::string_view value) {
	out += "    ";
	out += prop;
	out += ": ";
	out += value;
	out += ";\n";
}

static void append_declarations(std::string &out, const StyleProperties &p) {
	if (p.background_color) {
		decl(out, "background-color", fmt_color(*p.background_color));
	}
	if (p.border_color) {
		decl(out, "border-color", fmt_color(*p.border_color));
	}
	if (p.color) {
		decl(out, "color", fmt_color(*p.color));
	}
	if (p.border_width) {
		decl(out, "border-width", fmt_float(*p.border_width) + "px");
	}
	if (p.border_radius) {
		const Vec4 &r = *p.border_radius;
		std::string v;
		if (r.x == r.y && r.y == r.z && r.z == r.w) {
			v = fmt_float(r.x) + "px";
		} else {
			v = fmt_float(r.x) + "px " + fmt_float(r.y) + "px " + fmt_float(r.z) + "px " + fmt_float(r.w) + "px";
		}
		decl(out, "border-radius", v);
	}
	if (p.opacity) {
		decl(out, "opacity", fmt_float(*p.opacity));
	}

	if (p.display) {
		decl(out, "display", *p.display == Display::Flex ? "flex" : "none");
	}
	if (p.overflow) {
		const char *v = "visible";
		if (*p.overflow == Overflow::Hidden) {
			v = "hidden";
		} else if (*p.overflow == Overflow::Scroll) {
			v = "scroll";
		} else if (*p.overflow == Overflow::Clip) {
			v = "clip";
		}
		decl(out, "overflow", v);
	}

	if (p.width) {
		decl(out, "width", fmt_length(*p.width));
	}
	if (p.height) {
		decl(out, "height", fmt_length(*p.height));
	}
	if (p.min) {
		decl(out, "min", fmt_length(*p.min));
	}
	if (p.max) {
		decl(out, "max", fmt_length(*p.max));
	}
	if (p.min_width) {
		decl(out, "min-width", fmt_length(*p.min_width));
	}
	if (p.max_width) {
		decl(out, "max-width", fmt_length(*p.max_width));
	}
	if (p.min_height) {
		decl(out, "min-height", fmt_length(*p.min_height));
	}
	if (p.max_height) {
		decl(out, "max-height", fmt_length(*p.max_height));
	}

	if (p.padding) {
		decl(out, "padding", fmt_edges(*p.padding));
	}
	if (p.gap) {
		decl(out, "gap", fmt_float(*p.gap) + "px");
	}

	if (p.flex_direction) {
		const char *v = "row";
		switch (*p.flex_direction) {
		case FlexDirection::Column:
			v = "column";
			break;
		case FlexDirection::RowReverse:
			v = "row-reverse";
			break;
		case FlexDirection::ColumnReverse:
			v = "column-reverse";
			break;
		default:
			break;
		}
		decl(out, "flex-direction", v);
	}
	if (p.justify_content) {
		const char *v = "start";
		if (*p.justify_content == JustifyContent::Center) {
			v = "center";
		} else if (*p.justify_content == JustifyContent::End) {
			v = "end";
		}
		decl(out, "justify-content", v);
	}
	if (p.align_items) {
		const char *v = "start";
		switch (*p.align_items) {
		case AlignItems::End:
			v = "end";
			break;
		case AlignItems::Center:
			v = "center";
			break;
		case AlignItems::Stretch:
			v = "stretch";
			break;
		default:
			break;
		}
		decl(out, "align-items", v);
	}
	if (p.flex_grow) {
		decl(out, "flex-grow", fmt_float(*p.flex_grow));
	}
	if (p.flex_wrap) {
		decl(out, "flex-wrap", *p.flex_wrap == FlexWrap::Wrap ? "wrap" : "nowrap");
	}

	if (p.position) {
		const char *v = "static";
		if (*p.position == Position::Relative) {
			v = "relative";
		} else if (*p.position == Position::Absolute) {
			v = "absolute";
		}
		decl(out, "position", v);
	}
	if (p.top) {
		decl(out, "top", fmt_length(*p.top));
	}
	if (p.right) {
		decl(out, "right", fmt_length(*p.right));
	}
	if (p.bottom) {
		decl(out, "bottom", fmt_length(*p.bottom));
	}
	if (p.left) {
		decl(out, "left", fmt_length(*p.left));
	}
	if (p.z_index) {
		decl(out, "z-index", std::to_string(*p.z_index));
	}

	if (p.font_family) {
		decl(out, "font-family", *p.font_family);
	}
	if (p.font_size) {
		decl(out, "font-size", fmt_float(*p.font_size));
	}
	if (p.text_align) {
		const char *v = "left";
		if (*p.text_align == TextAlign::Center) {
			v = "center";
		} else if (*p.text_align == TextAlign::Right) {
			v = "right";
		}
		decl(out, "text-align", v);
	}

	if (p.transition_duration) {
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%.4gms", *p.transition_duration);
		decl(out, "transition-duration", buf);
	}
	if (p.transition_easing) {
		const char *v = "linear";
		switch (*p.transition_easing) {
		case TransitionEasing::Ease:
			v = "ease";
			break;
		case TransitionEasing::EaseIn:
			v = "ease-in";
			break;
		case TransitionEasing::EaseOut:
			v = "ease-out";
			break;
		case TransitionEasing::EaseInOut:
			v = "ease-in-out";
			break;
		default:
			break;
		}
		decl(out, "transition-easing", v);
	}

	if (p.box_shadows && !p.box_shadows->empty()) {
		std::string value;
		for (size_t i = 0; i < p.box_shadows->size(); ++i) {
			if (i > 0) {
				value += ", ";
			}
			const BoxShadow &sh = (*p.box_shadows)[i];
			if (sh.inset) {
				value += "inset ";
			}
			value += fmt_float(sh.offset.x) + "px ";
			value += fmt_float(sh.offset.y) + "px ";
			value += fmt_float(sh.blur) + "px ";
			value += fmt_float(sh.spread) + "px ";
			value += fmt_color(sh.color);
		}
		decl(out, "box-shadow", value);
	}
}

std::string Theme::to_aq_style() const {
	std::string out;
	out += "/* Generated from Theme — load via StyleParser::LoadString() or LoadFile() */\n";

	if (!m_colors.empty()) {
		out += "\n/* @palette — reference only, not loaded by the parser */\n";
		std::vector<std::pair<std::string, Vec4>> colors(m_colors.begin(), m_colors.end());
		std::ranges::sort(colors, {}, &std::pair<std::string, Vec4>::first);
		for (const auto &[name, color] : colors) {
			char buf[128];
			std::snprintf(buf, sizeof(buf), "/* @color %-24s %s */\n", (name + ":").c_str(), fmt_color(color).c_str());
			out += buf;
		}
	}

	if (!m_constants.empty()) {
		out += "\n/* @constants — reference only, not loaded by the parser */\n";
		std::vector<std::pair<std::string, float>> constants(m_constants.begin(), m_constants.end());
		std::ranges::sort(constants, {}, &std::pair<std::string, float>::first);
		for (const auto &[name, value] : constants) {
			char buf[64];
			std::snprintf(buf, sizeof(buf), "/* @const %-24s %.4g */\n", (name + ":").c_str(), value);
			out += buf;
		}
	}

	using Entry = const decltype(m_styles)::value_type *;
	std::vector<Entry> entries;
	entries.reserve(m_styles.size());
	for (const auto &kv : m_styles) {
		entries.push_back(&kv);
	}
	std::ranges::sort(entries, [](Entry a, Entry b) {
		if (a->first.type_name != b->first.type_name) {
			return a->first.type_name < b->first.type_name;
		}
		return a->first.pseudo_class < b->first.pseudo_class;
	});

	for (const auto *entry : entries) {
		const auto &[key, props] = *entry;
		out += "\n";
		out += key.type_name;
		if (!key.pseudo_class.empty()) {
			out += ":";
			out += key.pseudo_class;
		}
		out += " {\n";
		append_declarations(out, props);
		out += "}\n";
	}

	return out;
}

bool Theme::save_to_file(const std::string &path) const {
	return Platform::Filesystem::VirtualFileSystem::get()->write_text_file(path, to_aq_style());
}

} // namespace Aquila::UI
