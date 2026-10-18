#include "Aquila/UI/Style/StyleParserHelper.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/Macros.h"

namespace Aquila::UI::ParserHelper {

std::string_view trim_sv(std::string_view s) {
	while (!s.empty() && (std::isspace(static_cast<unsigned char>(s.front())) != 0)) {
		s.remove_prefix(1);
	}
	while (!s.empty() && (std::isspace(static_cast<unsigned char>(s.back())) != 0)) {
		s.remove_suffix(1);
	}
	return s;
}

std::string trim(std::string_view s) {
	return std::string(trim_sv(s));
}

std::vector<std::string> split(std::string_view s, char delim) {
	std::vector<std::string> result;
	while (!s.empty()) {
		size_t pos = s.find(delim);
		std::string_view tok = (pos == std::string_view::npos) ? s : s.substr(0, pos);
		std::string t = trim(tok);
		if (!t.empty()) {
			result.push_back(std::move(t));
		}
		if (pos == std::string_view::npos) {
			break;
		}
		s.remove_prefix(pos + 1);
	}
	return result;
}

std::vector<std::string> split_ws(std::string_view s) {
	std::vector<std::string> result;
	size_t i = 0;
	while (i < s.size()) {
		while (i < s.size() && (std::isspace(static_cast<unsigned char>(s[i])) != 0)) {
			++i;
		}
		size_t start = i;
		while (i < s.size() && (std::isspace(static_cast<unsigned char>(s[i])) == 0)) {
			++i;
		}
		if (i > start) {
			result.emplace_back(s.substr(start, i - start));
		}
	}
	return result;
}

std::string strip_comments(std::string_view src) {
	std::string out;
	out.reserve(src.size());
	size_t i = 0;
	while (i < src.size()) {
		if (i + 1 < src.size() && src[i] == '/' && src[i + 1] == '*') {
			i += 2;
			while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/')) {
				if (src[i] == '\n') {
					out += '\n';
				}
				++i;
			}
			if (i + 1 >= src.size()) {
				AQUILA_LOG_WARNING("StyleParser: a '/*' comment is never closed, the rest of the file is ignored");
			}
			i += 2;
		} else if (i + 1 < src.size() && src[i] == '/' && src[i + 1] == '/') {
			while (i < src.size() && src[i] != '\n') {
				++i;
			}
		} else {
			out += src[i++];
		}
	}
	return out;
}

float parse_float(std::string_view s) {
	s = trim_sv(s);
	float v = 0.F;
	std::from_chars(s.data(), s.data() + s.size(), v);
	return v;
}

float parse_duration_ms(std::string_view s) {
	s = trim_sv(s);
	if (s.ends_with("ms")) {
		return parse_float(s.substr(0, s.size() - 2));
	}
	if (s.ends_with('s')) {
		return parse_float(s.substr(0, s.size() - 1)) * 1000.F;
	}
	return parse_float(s);
}

std::optional<TransitionEasing> parse_easing(std::string_view s) {
	if (s == "linear") {
		return TransitionEasing::Linear;
	}
	if (s == "ease") {
		return TransitionEasing::Ease;
	}
	if (s == "ease-in") {
		return TransitionEasing::EaseIn;
	}
	if (s == "ease-out") {
		return TransitionEasing::EaseOut;
	}
	if (s == "ease-in-out") {
		return TransitionEasing::EaseInOut;
	}
	return std::nullopt;
}

Option<StyleLength> parse_length(std::string_view raw) {
	std::string s = to_lower(trim(raw));
	if (s == "auto") {
		return StyleLength::Auto();
	}
	if (s == "grow" || s == "1fr") {
		return StyleLength::grow();
	}
	if (s.ends_with("px")) {
		return StyleLength::pixel(parse_float(std::string_view(s).substr(0, s.size() - 2)));
	}
	if (s.ends_with("%")) {
		return StyleLength::percent(parse_float(std::string_view(s).substr(0, s.size() - 1)));
	}
	if (s.ends_with("vw")) {
		return StyleLength::vw(parse_float(std::string_view(s).substr(0, s.size() - 2)));
	}
	if (s.ends_with("vh")) {
		return StyleLength::vh(parse_float(std::string_view(s).substr(0, s.size() - 2)));
	}
	if (!s.empty() && ((std::isdigit(static_cast<unsigned char>(s[0])) != 0) || s[0] == '-' || s[0] == '.')) {
		return StyleLength::pixel(parse_float(s));
	}
	return std::nullopt;
}

Option<Vec4> parse_color(std::string_view raw) {
	std::string s = to_lower(trim(raw));

	if (s == "transparent") {
		return Vec4(0.F);
	}

	if (s.starts_with('#')) {
		auto hex = std::string_view(s).substr(1);
		auto byte = [&](size_t off) -> float {
			uint8_t v = 0;
			std::from_chars(hex.data() + off, hex.data() + off + 2, v, 16);
			return v / 255.F;
		};
		if (hex.size() == 6) {
			return Vec4(byte(0), byte(2), byte(4), 1.F);
		}
		if (hex.size() == 8) {
			return Vec4(byte(0), byte(2), byte(4), byte(6));
		}
		return std::nullopt;
	}

	bool has_alpha = s.starts_with("rgba(");
	if (has_alpha || s.starts_with("rgb(")) {
		size_t open = s.find('(');
		size_t close = s.rfind(')');
		if (open == std::string::npos || close == std::string::npos) {
			return std::nullopt;
		}
		auto parts = split(std::string_view(s).substr(open + 1, close - open - 1), ',');
		if (parts.size() < 3) {
			return std::nullopt;
		}
		float r = parse_float(parts[0]);
		float g = parse_float(parts[1]);
		float b = parse_float(parts[2]);
		float a = (has_alpha && parts.size() >= 4) ? parse_float(parts[3]) : 1.F;
		if (r > 1.F || g > 1.F || b > 1.F) {
			r /= 255.F;
			g /= 255.F;
			b /= 255.F;
		}
		return Vec4(r, g, b, a);
	}

	return std::nullopt;
}

Option<StyleEdges> parse_edges(std::string_view s) {
	auto parts = split_ws(s);
	if (parts.empty()) {
		return std::nullopt;
	}

	auto l0 = parse_length(parts[0]);
	if (!l0) {
		return std::nullopt;
	}
	if (parts.size() == 1) {
		return StyleEdges::all(*l0);
	}

	auto l1 = parse_length(parts[1]);
	if (!l1) {
		return std::nullopt;
	}
	if (parts.size() == 2) {
		return StyleEdges::axes(*l0, *l1);
	}

	if (parts.size() >= 4) {
		auto l2 = parse_length(parts[2]);
		auto l3 = parse_length(parts[3]);
		if (!l2 || !l3) {
			return std::nullopt;
		}
		return StyleEdges{ .top = *l0, .right = *l1, .bottom = *l2, .left = *l3 };
	}

	return std::nullopt;
}

Option<Vec4> parse_radius(std::string_view s) {
	auto parts = split_ws(s);
	if (parts.empty()) {
		return std::nullopt;
	}

	auto px = [](Option<StyleLength> l) -> float { return (l && l->unit == LengthUnit::Pixel) ? l->value : 0.F; };

	if (parts.size() == 1) {
		float v = px(parse_length(parts[0]));
		return Vec4(v);
	}
	if (parts.size() >= 4) {
		return Vec4(px(parse_length(parts[0])), px(parse_length(parts[1])), px(parse_length(parts[2])),
					px(parse_length(parts[3])));
	}
	return std::nullopt;
}

Option<BoxShadow> parse_one_shadow(std::string_view raw) {
	std::vector<std::string> tokens;
	size_t i = 0;
	std::string s = trim(raw);
	while (i < s.size()) {
		while (i < s.size() && (std::isspace(static_cast<unsigned char>(s[i])) != 0)) {
			++i;
		}
		if (i >= s.size()) {
			break;
		}
		size_t start = i;
		if (s[i] == 'r' && i + 3 < s.size() && s.substr(i, 3) == "rgb") {
			while (i < s.size() && s[i] != ')') {
				++i;
			}
			if (i < s.size()) {
				++i;
			}
		} else {
			while (i < s.size() && (std::isspace(static_cast<unsigned char>(s[i])) == 0)) {
				++i;
			}
		}
		tokens.emplace_back(s.substr(start, i - start));
	}

	BoxShadow out;
	std::vector<std::string> lengths;
	for (const auto &tok : tokens) {
		if (tok == "inset") {
			out.inset = true;
			continue;
		}
		if (auto c = parse_color(tok)) {
			out.color = *c;
			continue;
		}
		lengths.push_back(tok);
	}
	auto px = [](std::string_view v) -> float {
		std::string t = trim(v);
		if (t.ends_with("px")) {
			t = t.substr(0, t.size() - 2);
		}
		float f = 0.F;
		std::from_chars(t.data(), t.data() + t.size(), f);
		return f;
	};
	if (lengths.size() < 2) {
		return std::nullopt;
	}
	out.offset.x = px(lengths[0]);
	out.offset.y = px(lengths[1]);
	if (lengths.size() >= 3) {
		out.blur = Math::max(0.F, px(lengths[2]));
	}
	if (lengths.size() >= 4) {
		out.spread = px(lengths[3]);
	}
	return out;
}

void apply_declaration(StyleProperties &props, std::string_view prop_raw, std::string_view value_raw) {
	std::string prop = to_lower(trim(prop_raw));
	std::string value = trim(value_raw);
	std::string value_lower = to_lower(value);

	// CSS custom properties (--name) are handled by the variable extraction pass;
	// silently ignore them here so no spurious warnings are emitted.
	if (prop.size() >= 2 && prop[0] == '-' && prop[1] == '-') {
		return;
	}

	if (prop == "background-color" || prop == "background") {
		if (auto color = parse_color(value)) {
			props.background_color = *color;
		}
	} else if (prop == "border-color") {
		if (auto color = parse_color(value)) {
			props.border_color = *color;
		}
	} else if (prop == "color") {
		if (auto color = parse_color(value)) {
			props.color = *color;
		}
	} else if (prop == "accent-color") {
		if (auto color = parse_color(value)) {
			props.accent_color = *color;
		}
	} else if (prop == "selection-color") {
		if (auto color = parse_color(value)) {
			props.selection_color = *color;
		}
	} else if (prop == "placeholder-color") {
		if (auto color = parse_color(value)) {
			props.placeholder_color = *color;
		}

		// ── border ──
	} else if (prop == "border-width") {
		props.border_width = parse_float(value);
	} else if (prop == "border-radius") {
		if (auto radius = parse_radius(value)) {
			props.border_radius = *radius;
		}
	} else if (prop == "border-style") {
		if (value_lower == "solid") {
			props.border_style = BorderStyle::Solid;
		} else if (value_lower == "dashed") {
			props.border_style = BorderStyle::Dashed;
		} else if (value_lower == "dotted") {
			props.border_style = BorderStyle::Dotted;
		}

	} else if (prop == "cursor") {
		if (value_lower == "text" || value_lower == "ibeam") {
			props.cursor = Platform::CursorType::Text;
		} else if (value_lower == "pointer" || value_lower == "hand") {
			props.cursor = Platform::CursorType::Hand;
		} else if (value_lower == "crosshair") {
			props.cursor = Platform::CursorType::Crosshair;
		} else if (value_lower == "resize-h" || value_lower == "ew-resize" || value_lower == "col-resize") {
			props.cursor = Platform::CursorType::ResizeHorizontal;
		} else if (value_lower == "resize-v" || value_lower == "ns-resize" || value_lower == "row-resize") {
			props.cursor = Platform::CursorType::ResizeVertical;
		} else if (value_lower == "nwse-resize" || value_lower == "resize-nwse") {
			props.cursor = Platform::CursorType::ResizeDiagonalTLBR;
		} else if (value_lower == "nesw-resize" || value_lower == "resize-nesw") {
			props.cursor = Platform::CursorType::ResizeDiagonalBLTR;
		} else if (value_lower == "move" || value_lower == "all-scroll") {
			props.cursor = Platform::CursorType::ResizeAll;
		} else if (value_lower == "not-allowed") {
			props.cursor = Platform::CursorType::NotAllowed;
		} else {
			props.cursor = Platform::CursorType::Arrow;
		}

		// ── opacity / display / overflow ──
	} else if (prop == "opacity") {
		props.opacity = parse_float(value);
	} else if (prop == "display") {
		if (value_lower == "flex") {
			props.display = Display::Flex;
		} else if (value_lower == "none") {
			props.display = Display::None;
		}
	} else if (prop == "overflow") {
		if (value_lower == "visible") {
			props.overflow = Overflow::Visible;
		} else if (value_lower == "hidden") {
			props.overflow = Overflow::Hidden;
		} else if (value_lower == "scroll") {
			props.overflow = Overflow::Scroll;
		} else if (value_lower == "clip") {
			props.overflow = Overflow::Clip;
		}

		// ── sizing ──
	} else if (prop == "width") {
		if (auto l = parse_length(value)) {
			props.width = *l;
		}
	} else if (prop == "height") {
		if (auto l = parse_length(value)) {
			props.height = *l;
		}
	} else if (prop == "min-width") {
		if (auto l = parse_length(value)) {
			props.min_width = *l;
		}
	} else if (prop == "max-width") {
		if (auto l = parse_length(value)) {
			props.max_width = *l;
		}
	} else if (prop == "min-height") {
		if (auto l = parse_length(value)) {
			props.min_height = *l;
		}
	} else if (prop == "max-height") {
		if (auto l = parse_length(value)) {
			props.max_height = *l;
		}
	} else if (prop == "min") {
		if (auto l = parse_length(value)) {
			props.min = *l;
		}
	} else if (prop == "max") {
		if (auto l = parse_length(value)) {
			props.max = *l;
		}

		// ── box model ──
	} else if (prop == "gap") {
		if (auto l = parse_length(value)) {
			props.gap = l->unit == LengthUnit::Pixel ? l->value : 0.F;
		}
	} else if (prop == "aspect-ratio") {
		const size_t slash = value.find('/');
		if (slash != std::string_view::npos) {
			const float w = parse_float(value.substr(0, slash));
			const float h = parse_float(value.substr(slash + 1));
			props.aspect_ratio = (h != 0.F) ? (w / h) : 0.F;
		} else {
			props.aspect_ratio = parse_float(value);
		}
	} else if (prop == "padding") {
		if (auto e = parse_edges(value)) {
			props.padding = *e;
		}
	} else if (prop == "padding-left") {
		if (auto l = parse_length(value)) {
			props.padding_left = *l;
		}
	} else if (prop == "padding-right") {
		if (auto l = parse_length(value)) {
			props.padding_right = *l;
		}
	} else if (prop == "padding-top") {
		if (auto l = parse_length(value)) {
			props.padding_top = *l;
		}
	} else if (prop == "padding-bottom") {
		if (auto l = parse_length(value)) {
			props.padding_bottom = *l;
		}

		// ── flex ──
	} else if (prop == "flex-direction") {
		if (value_lower == "row") {
			props.flex_direction = FlexDirection::Row;
		} else if (value_lower == "column") {
			props.flex_direction = FlexDirection::Column;
		} else if (value_lower == "row-reverse") {
			props.flex_direction = FlexDirection::RowReverse;
		} else if (value_lower == "column-reverse") {
			props.flex_direction = FlexDirection::ColumnReverse;
		}
	} else if (prop == "justify-content") {
		if (value_lower == "start" || value_lower == "flex-start") {
			props.justify_content = JustifyContent::Start;
		} else if (value_lower == "end" || value_lower == "flex-end") {
			props.justify_content = JustifyContent::End;
		} else if (value_lower == "center") {
			props.justify_content = JustifyContent::Center;
		} else {
			AQUILA_LOG_WARNING("JustifyContent : {} is unsupported", value_lower);
		}
	} else if (prop == "align-items") {
		if (value_lower == "start" || value_lower == "flex-start") {
			props.align_items = AlignItems::Start;
		} else if (value_lower == "end" || value_lower == "flex-end") {
			props.align_items = AlignItems::End;
		} else if (value_lower == "center") {
			props.align_items = AlignItems::Center;
		} else if (value_lower == "stretch") {
			props.align_items = AlignItems::Stretch;
		}
	} else if (prop == "flex-grow") {
		props.flex_grow = parse_float(value);
	} else if (prop == "flex-wrap") {
		if (value_lower == "nowrap" || value_lower == "no-wrap") {
			props.flex_wrap = FlexWrap::NoWrap;
		} else if (value_lower == "wrap") {
			props.flex_wrap = FlexWrap::Wrap;
		}

	} else if (prop == "position") {
		if (value_lower == "static") {
			props.position = Position::Static;
		} else if (value_lower == "relative") {
			props.position = Position::Relative;
		} else if (value_lower == "absolute") {
			props.position = Position::Absolute;
		}
	} else if (prop == "top") {
		if (auto l = parse_length(value)) {
			props.top = *l;
		}
	} else if (prop == "right") {
		if (auto l = parse_length(value)) {
			props.right = *l;
		}
	} else if (prop == "bottom") {
		if (auto l = parse_length(value)) {
			props.bottom = *l;
		}
	} else if (prop == "left") {
		if (auto l = parse_length(value)) {
			props.left = *l;
		}
	} else if (prop == "z-index") {
		props.z_index = static_cast<Int32>(parse_float(value));

	} else if (prop == "font-family") {
		props.font_family = std::string(value);
	} else if (prop == "font-size") {
		static const std::unordered_map<std::string_view, FontSize> k_font_size_names = {
			{ "Tiny", FontSize::Tiny },			  { "XSmall", FontSize::XSmall },
			{ "Small", FontSize::Small },		  { "Body", FontSize::Body },
			{ "BodyLarge", FontSize::BodyLarge }, { "Subtitle", FontSize::Subtitle },
			{ "Heading", FontSize::Heading },	  { "HeadingLarge", FontSize::HeadingLarge },
			{ "Title", FontSize::Title },		  { "TitleLarge", FontSize::TitleLarge },
			{ "Display", FontSize::Display },	  { "DisplayLarge", FontSize::DisplayLarge },
		};
		if (auto it = k_font_size_names.find(value); it != k_font_size_names.end()) {
			props.font_size = font_size_to_pixels(it->second);
		} else {
			props.font_size = parse_float(value);
		}

		// ── transitions ──
	} else if (prop == "transition-duration") {
		props.transition_duration = parse_duration_ms(value);
	} else if (prop == "transition-easing" || prop == "transition-timing-function") {
		if (auto easing = parse_easing(value)) {
			props.transition_easing = easing;
		}
	} else if (prop == "text-align") {
		if (value_lower == "left") {
			props.text_align = TextAlign::Left;
		} else if (value_lower == "center") {
			props.text_align = TextAlign::Center;
		} else if (value_lower == "right") {
			props.text_align = TextAlign::Right;
		}
	} else if (prop == "white-space") {
		if (value_lower == "normal") {
			props.white_space = WhiteSpace::Normal;
		} else if (value_lower == "nowrap" || value_lower == "no-wrap") {
			props.white_space = WhiteSpace::Nowrap;
		}

	} else if (prop == "box-shadow") {
		if (value_lower == "none") {
			props.box_shadows = std::vector<BoxShadow>{};
		} else {
			std::vector<BoxShadow> shadows;
			std::string layer;
			int depth = 0;
			for (char c : value) {
				if (c == '(') {
					++depth;
					layer += c;
				} else if (c == ')') {
					--depth;
					layer += c;
				} else if (c == ',' && depth == 0) {
					if (auto sh = parse_one_shadow(layer)) {
						shadows.push_back(*sh);
					}
					layer.clear();
				} else {
					layer += c;
				}
			}
			if (!layer.empty()) {
				if (auto sh = parse_one_shadow(layer)) {
					shadows.push_back(*sh);
				}
			}
			if (!shadows.empty()) {
				props.box_shadows = std::move(shadows);
			}
		}

	} else if (prop == "transition") {
		for (std::string_view token : split(value, ' ')) {
			token = trim_sv(token);
			if (token.empty() || token == "all") {
				continue;
			}
			if (auto easing = parse_easing(token)) {
				props.transition_easing = easing;
			} else {
				if (token.ends_with("ms") || token.ends_with('s') ||
					(std::isdigit(static_cast<unsigned char>(token.front())) != 0)) {
					props.transition_duration = parse_duration_ms(token);
				}
			}
		}

	} else {
		AQUILA_LOG_WARNING("StyleParser: unknown property '{}'", prop);
	}
}

void parse_block(std::string_view selector_list, std::string_view body, StyleSheet &sheet) {
	StyleProperties props;
	const std::string selectors = trim(selector_list);

	for (const auto &decl : split(body, ';')) {
		const size_t colon = decl.find(':');
		if (colon == std::string::npos) {
			AQUILA_LOG_WARNING("StyleParser: '{}': declaration '{}' is missing ':'", selectors, decl);
			continue;
		}
		const std::string_view property = trim_sv(std::string_view(decl).substr(0, colon));
		const std::string_view value = trim_sv(std::string_view(decl).substr(colon + 1));
		if (property.empty() || value.empty()) {
			AQUILA_LOG_WARNING("StyleParser: '{}': declaration '{}' needs 'property: value'", selectors, decl);
			continue;
		}
		apply_declaration(props, property, value);
	}

	for (const auto &sel : split(selector_list, ',')) {
		if (sel.find_first_of(" \t\r\n>+~*[") != std::string::npos) {
			AQUILA_LOG_WARNING(
				"StyleParser: unsupported selector '{}', use Type, .class or #id with an optional :state", sel);
			continue;
		}

		std::string s = sel;
		std::string pseudo_class;
		if (const size_t pseudo_pos = s.find(':'); pseudo_pos != std::string::npos) {
			pseudo_class = to_lower(s.substr(pseudo_pos + 1));
			s.resize(pseudo_pos);
		}

		if (pseudo_class == "root") {
			if (!s.empty()) {
				AQUILA_LOG_WARNING("StyleParser: unsupported selector '{}', ':root' stands alone", sel);
			}
			continue;
		}
		if (!pseudo_class.empty() && !StyleSheet::supports_pseudo_class(pseudo_class)) {
			AQUILA_LOG_WARNING("StyleParser: unknown state ':{}' in '{}'", pseudo_class, sel);
			continue;
		}

		StyleRule::SelectorType type;
		std::string name;
		if (s.starts_with('#')) {
			type = StyleRule::SelectorType::Id;
			name = s.substr(1);
		} else if (s.starts_with('.')) {
			type = StyleRule::SelectorType::Class;
			name = s.substr(1);
		} else {
			type = StyleRule::SelectorType::Type;
			name = s;
		}

		if (name.empty()) {
			AQUILA_LOG_WARNING("StyleParser: selector '{}' needs a type, class or id", sel);
			continue;
		}

		sheet.add_rule(type, std::move(name), std::move(pseudo_class), props);
	}
}

} // namespace Aquila::UI::ParserHelper
