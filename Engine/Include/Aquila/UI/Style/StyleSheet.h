#pragma once

#include "Aquila/UI/Style/ComputedStyle.h"
#include "Aquila/UI/Style/StyleProperties.h"

namespace Aquila::UI::Core {
class View;
}

namespace Aquila::UI {

struct StyleRule {
	enum class SelectorType : Uint8 { Type, Class, Id };

	SelectorType selector_type;
	std::string selector;
	std::string pseudo_class;
	Int32 specificity = 0;
	StyleProperties properties;
};

struct MediaCondition {
	enum class Axis { Width, Height };
	enum class Op { Less, LessEq, Greater, GreaterEq, Equal };

	Axis axis = Axis::Width;
	Op op = Op::Less;
	float value = 0.F;

	[[nodiscard]] bool evaluate(float w, float h) const;
};

struct MediaBlock {
	std::vector<MediaCondition> conditions;
	std::vector<StyleRule> rules;
};

struct StyleResolveContext {
	Vec2 viewport_size = {};
	Vec2 container_size = {};
};

class StyleSheet {
  public:
	using ResolveContext = StyleResolveContext;

	void add_rule(StyleRule::SelectorType type, std::string selector, std::string pseudo_class, StyleProperties props);

	void add_media_block(MediaBlock block);
	void add_container_block(MediaBlock block);

	void add_variable(std::string name, std::string value);
	[[nodiscard]] const std::string *get_variable(std::string_view name) const;
	[[nodiscard]] const std::unordered_map<std::string, std::string> &get_variables() const { return m_variables; }

	[[nodiscard]] const std::vector<StyleRule> &get_rules() const { return m_rules; }
	[[nodiscard]] const std::vector<MediaBlock> &get_media_blocks() const { return m_media_blocks; }
	[[nodiscard]] const std::vector<MediaBlock> &get_container_blocks() const { return m_container_blocks; }

	[[nodiscard]] bool has_media_blocks() const { return !m_media_blocks.empty(); }
	[[nodiscard]] bool has_container_blocks() const { return !m_container_blocks.empty(); }

	[[nodiscard]] ComputedStyle resolve(const Core::View &view, const ComputedStyle *parent_computed,
										const ResolveContext &ctx = {}) const;

	[[nodiscard]] Usize get_rule_count() const { return m_rules.size(); }

	AQUILA_INLINE void clear() {
		m_rules.clear();
		m_media_blocks.clear();
		m_container_blocks.clear();
		m_variables.clear();
	}

  private:
	[[nodiscard]] bool matches(const StyleRule &rule, const Core::View &view) const;

	void apply_properties(ComputedStyle &out, const StyleProperties &props, bool scale_metrics) const;
	void apply_matching_rules(ComputedStyle &out, const std::vector<StyleRule> &rules, const Core::View &view) const;

	std::vector<StyleRule> m_rules;
	std::vector<MediaBlock> m_media_blocks;
	std::vector<MediaBlock> m_container_blocks;
	std::unordered_map<std::string, std::string> m_variables;
};

} // namespace Aquila::UI
