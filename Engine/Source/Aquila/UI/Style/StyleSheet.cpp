#include "Aquila/UI/Style/StyleSheet.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/UI/Core/FontRegistry.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Style/StylePropertyList.h"

namespace Aquila::UI {

bool MediaCondition::evaluate(float w, float h) const {
	const float v = (axis == Axis::Width) ? w : h;
	switch (op) {
	case Op::Less:
		return v < value;
	case Op::LessEq:
		return v <= value;
	case Op::Greater:
		return v > value;
	case Op::GreaterEq:
		return v >= value;
	case Op::Equal:
		return Math::abs(v - value) < 0.5F;
	}
	return false;
}

static bool evaluate_block(const MediaBlock &block, float w, float h) {
	for (const auto &cond : block.conditions) {
		if (!cond.evaluate(w, h)) {
			return false;
		}
	}
	return true;
}

void StyleSheet::add_rule(StyleRule::SelectorType type, std::string selector, std::string pseudo_class,
						 StyleProperties props) {
	Int32 specificity = 0;
	switch (type) {
	case StyleRule::SelectorType::Type:
		specificity = 1;
		break;
	case StyleRule::SelectorType::Class:
		specificity = 10;
		break;
	case StyleRule::SelectorType::Id:
		specificity = 100;
		break;
	}
	if (!pseudo_class.empty()) {
		specificity += 10;
	}

	m_rules.push_back({ type, std::move(selector), std::move(pseudo_class), specificity, std::move(props) });
}

void StyleSheet::add_media_block(MediaBlock block) {
	m_media_blocks.push_back(std::move(block));
}

void StyleSheet::add_container_block(MediaBlock block) {
	m_container_blocks.push_back(std::move(block));
}

void StyleSheet::add_variable(std::string name, std::string value) {
	m_variables[std::move(name)] = std::move(value);
}

const std::string *StyleSheet::get_variable(std::string_view name) const {
	const auto it = m_variables.find(std::string(name));
	return it != m_variables.end() ? &it->second : nullptr;
}

void StyleSheet::apply_matching_rules(ComputedStyle &out, const std::vector<StyleRule> &rules,
									const Core::View &view) const {
	std::vector<const StyleRule *> matching;
	for (const auto &rule : rules) {
		if (matches(rule, view)) {
			matching.push_back(&rule);
		}
	}
	std::ranges::stable_sort(matching, {}, [](const StyleRule *r) { return r->specificity; });
	for (const StyleRule *rule : matching) {
		apply_properties(out, rule->properties, true);
	}
}

ComputedStyle StyleSheet::resolve(const Core::View &view, const ComputedStyle *parent_computed,
								  const ResolveContext &ctx) const {
	ComputedStyle result;

	if (parent_computed != nullptr) {
#define AQ_STYLE_PROP(css, sp, cs, layout, anim, inherit) \
	AQ_STYLE_WHEN(inherit, result.cs = parent_computed->cs;)
		AQ_STYLE_PROPERTY_LIST
#undef AQ_STYLE_PROP
	}

	std::vector<const StyleRule *> matching;
	for (const auto &rule : m_rules) {
		if (matches(rule, view)) {
			matching.push_back(&rule);
		}
	}
	std::ranges::stable_sort(matching, {}, [](const StyleRule *r) { return r->specificity; });
	for (const StyleRule *rule : matching) {
		apply_properties(result, rule->properties, true);
	}

	for (const auto &block : m_media_blocks) {
		if (evaluate_block(block, ctx.viewport_size.x, ctx.viewport_size.y)) {
			apply_matching_rules(result, block.rules, view);
		}
	}

	for (const auto &block : m_container_blocks) {
		if (evaluate_block(block, ctx.container_size.x, ctx.container_size.y)) {
			apply_matching_rules(result, block.rules, view);
		}
	}

	apply_properties(result, view.get_style(), false);

	return result;
}

bool StyleSheet::matches(const StyleRule &rule, const Core::View &view) const {
	bool selector_match = false;
	switch (rule.selector_type) {
	case StyleRule::SelectorType::Type:
		selector_match = (view.get_type_name() == rule.selector);
		break;
	case StyleRule::SelectorType::Class: {
		const auto &classes = view.get_classes();
		selector_match = (std::ranges::find(classes, rule.selector) != classes.end());
		break;
	}
	case StyleRule::SelectorType::Id:
		selector_match = (view.get_id() == rule.selector);
		break;
	}

	if (!selector_match) {
		return false;
	}

	if (rule.pseudo_class.empty()) {
		return true;
	}
	if (rule.pseudo_class == "hover") {
		return view.is_hovered();
	}
	if (rule.pseudo_class == "pressed") {
		return view.is_pressed();
	}
	if (rule.pseudo_class == "focus") {
		return view.is_focused();
	}
	if (rule.pseudo_class == "disabled") {
		return !view.is_enabled();
	}

	return false;
}

bool StyleSheet::supports_pseudo_class(std::string_view pseudo_class) {
	return pseudo_class == "hover" || pseudo_class == "pressed" || pseudo_class == "focus" ||
		pseudo_class == "disabled";
}

namespace {

StyleLength scale_length(StyleLength length, float scale) {
	if (length.unit == LengthUnit::Pixel) {
		length.value *= scale;
	}
	return length;
}

StyleEdges scale_edges(StyleEdges edges, float scale) {
	return { .top = scale_length(edges.top, scale),
			 .right = scale_length(edges.right, scale),
			 .bottom = scale_length(edges.bottom, scale),
			 .left = scale_length(edges.left, scale) };
}

} // namespace

void StyleSheet::apply_properties(ComputedStyle &out, const StyleProperties &props, bool scale_metrics) const {
	const float scale = scale_metrics ? Core::FontRegistry::ui_scale() : 1.F;

	if (props.min) {
		out.min_width = scale_length(*props.min, scale);
		out.min_height = scale_length(*props.min, scale);
	}
	if (props.max) {
		out.max_width = scale_length(*props.max, scale);
		out.max_height = scale_length(*props.max, scale);
	}

#define AQ_STYLE_PROP(css, sp, cs, layout, anim, inherit) \
	if (props.sp) {                                        \
		out.cs = *props.sp;                                \
	}
	AQ_STYLE_PROPERTY_LIST
#undef AQ_STYLE_PROP

	if (scale != 1.F) {
		if (props.width) {
			out.width = scale_length(*props.width, scale);
		}
		if (props.height) {
			out.height = scale_length(*props.height, scale);
		}
		if (props.min_width) {
			out.min_width = scale_length(*props.min_width, scale);
		}
		if (props.max_width) {
			out.max_width = scale_length(*props.max_width, scale);
		}
		if (props.min_height) {
			out.min_height = scale_length(*props.min_height, scale);
		}
		if (props.max_height) {
			out.max_height = scale_length(*props.max_height, scale);
		}
		if (props.padding) {
			out.padding = scale_edges(*props.padding, scale);
		}
		if (props.gap) {
			out.gap = *props.gap * scale;
		}
		if (props.border_width) {
			out.border_width = *props.border_width * scale;
		}
		if (props.border_radius) {
			out.border_radius = *props.border_radius * scale;
		}
		if (props.top) {
			out.top = scale_length(*props.top, scale);
		}
		if (props.right) {
			out.right = scale_length(*props.right, scale);
		}
		if (props.bottom) {
			out.bottom = scale_length(*props.bottom, scale);
		}
		if (props.left) {
			out.left = scale_length(*props.left, scale);
		}
		if (props.box_shadows) {
			out.box_shadows = *props.box_shadows;
			for (BoxShadow &shadow : out.box_shadows) {
				shadow.offset *= scale;
				shadow.blur *= scale;
				shadow.spread *= scale;
			}
		}
	}

	if (props.font_size) {
		out.font_size = *props.font_size * Core::FontRegistry::ui_scale();
	}

	if (props.padding_left) {
		out.padding.left = scale_length(*props.padding_left, scale);
	}
	if (props.padding_right) {
		out.padding.right = scale_length(*props.padding_right, scale);
	}
	if (props.padding_top) {
		out.padding.top = scale_length(*props.padding_top, scale);
	}
	if (props.padding_bottom) {
		out.padding.bottom = scale_length(*props.padding_bottom, scale);
	}
}

} // namespace Aquila::UI
