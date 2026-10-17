#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Style/StyleSheet.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"

namespace Aquila::UI {

static size_t find_matching_close(const std::string &src, size_t open_pos) {
	int depth = 1;
	size_t i = open_pos + 1;
	while (i < src.size() && depth > 0) {
		if (src[i] == '{') {
			++depth;
		} else if (src[i] == '}') {
			--depth;
		}
		++i;
	}
	return depth == 0 ? i - 1 : std::string::npos;
}

static std::unordered_map<std::string, std::string> extract_variables(std::string_view src) {
	std::unordered_map<std::string, std::string> vars;
	size_t i = 0;
	while (i < src.size()) {
		size_t dash_pos = src.find("--", i);
		if (dash_pos == std::string_view::npos) {
			break;
		}

		size_t colon_pos = src.find(':', dash_pos);
		size_t semi_pos = src.find(';', dash_pos);
		size_t close_pos = src.find('}', dash_pos);

		if (colon_pos == std::string_view::npos || semi_pos == std::string_view::npos) {
			break;
		}

		if (colon_pos > semi_pos || colon_pos > close_pos) {
			i = dash_pos + 2;
			continue;
		}

		std::string name = ParserHelper::trim(src.substr(dash_pos, colon_pos - dash_pos));
		std::string value = ParserHelper::trim(src.substr(colon_pos + 1, semi_pos - colon_pos - 1));

		if (name.size() > 2 && name[0] == '-' && name[1] == '-') {
			vars[std::move(name)] = std::move(value);
		}
		i = semi_pos + 1;
	}
	return vars;
}

static std::string substitute_variables(std::string src, const std::unordered_map<std::string, std::string> &vars) {
	for (const auto &[name, value] : vars) {
		std::string needle = "var(" + name + ")";
		size_t pos = 0;
		while ((pos = src.find(needle, pos)) != std::string::npos) {
			src.replace(pos, needle.size(), value);
			pos += value.size();
		}
	}
	return src;
}

static Option<MediaCondition> parse_media_condition(std::string_view raw) {
	using Axis = MediaCondition::Axis;
	using Op = MediaCondition::Op;

	const std::string c = ParserHelper::trim(raw);
	if (c.empty()) {
		return std::nullopt;
	}

	const size_t colon_pos = c.find(':');
	if (colon_pos != std::string::npos) {
		const std::string prop = ParserHelper::trim(c.substr(0, colon_pos));
		const float val = ParserHelper::parse_float(ParserHelper::trim(c.substr(colon_pos + 1)));
		if (prop == "min-width") {
			return MediaCondition{ .axis = Axis::Width, .op = Op::GreaterEq, .value = val };
		}
		if (prop == "max-width") {
			return MediaCondition{ .axis = Axis::Width, .op = Op::LessEq, .value = val };
		}
		if (prop == "min-height") {
			return MediaCondition{ .axis = Axis::Height, .op = Op::GreaterEq, .value = val };
		}
		if (prop == "max-height") {
			return MediaCondition{ .axis = Axis::Height, .op = Op::LessEq, .value = val };
		}
		if (prop == "width") {
			return MediaCondition{ .axis = Axis::Width, .op = Op::Equal, .value = val };
		}
		if (prop == "height") {
			return MediaCondition{ .axis = Axis::Height, .op = Op::Equal, .value = val };
		}
		return std::nullopt;
	}

	Axis axis;
	size_t axis_end = 0;
	if (c.size() >= 6 && c.substr(0, 6) == "height") {
		axis = Axis::Height;
		axis_end = 6;
	} else if (c.size() >= 5 && c.substr(0, 5) == "width") {
		axis = Axis::Width;
		axis_end = 5;
	} else {
		return std::nullopt;
	}

	const std::string rest = ParserHelper::trim(c.substr(axis_end));
	if (rest.empty()) {
		return std::nullopt;
	}

	Op op;
	size_t op_end = 0;
	if (rest.size() >= 2 && rest.substr(0, 2) == "<=") {
		op = Op::LessEq;
		op_end = 2;
	} else if (rest.size() >= 2 && rest.substr(0, 2) == ">=") {
		op = Op::GreaterEq;
		op_end = 2;
	} else if (rest[0] == '<') {
		op = Op::Less;
		op_end = 1;
	} else if (rest[0] == '>') {
		op = Op::Greater;
		op_end = 1;
	} else {
		return std::nullopt;
	}

	const float val = ParserHelper::parse_float(ParserHelper::trim(rest.substr(op_end)));
	return MediaCondition{ .axis = axis, .op = op, .value = val };
}

static void parse_at_block(std::string_view cond_text, std::string_view body, StyleSheet &sheet, bool is_container) {
	std::vector<MediaCondition> conditions;
	const std::string cond_str = ParserHelper::trim(cond_text);
	size_t j = 0;
	while (j < cond_str.size()) {
		while (j < cond_str.size() && (std::isspace((unsigned char)cond_str[j]) != 0)) {
			++j;
		}
		if (j >= cond_str.size()) {
			break;
		}

		if (cond_str[j] == '(') {
			const size_t close_p = cond_str.find(')', j);
			if (close_p == std::string::npos) {
				break;
			}
			auto cond = parse_media_condition(std::string_view(cond_str).substr(j + 1, close_p - j - 1));
			if (cond) {
				conditions.push_back(*cond);
			}
			j = close_p + 1;
		} else {
			while (j < cond_str.size() && cond_str[j] != '(') {
				++j;
			}
		}
	}
	if (conditions.empty()) {
		return;
	}

	StyleSheet temp_sheet;
	size_t k = 0;
	while (k < body.size()) {
		while (k < body.size() && (std::isspace((unsigned char)body[k]) != 0)) {
			++k;
		}
		if (k >= body.size()) {
			break;
		}

		const size_t inner_open = body.find('{', k);
		if (inner_open == std::string_view::npos) {
			break;
		}

		const std::string inner_sel = ParserHelper::trim(body.substr(k, inner_open - k));

		const size_t inner_close = body.find('}', inner_open + 1);
		if (inner_close == std::string_view::npos) {
			break;
		}

		if (!inner_sel.empty() && inner_sel[0] != '@') {
			const std::string_view inner_body = body.substr(inner_open + 1, inner_close - inner_open - 1);
			ParserHelper::parse_block(inner_sel, inner_body, temp_sheet);
		}
		k = inner_close + 1;
	}

	MediaBlock block;
	block.conditions = std::move(conditions);
	block.rules = temp_sheet.get_rules();

	if (is_container) {
		sheet.add_container_block(std::move(block));
	} else {
		sheet.add_media_block(std::move(block));
	}
}

void StyleParser::apply_property(StyleProperties &props, std::string_view property, std::string_view value) {
	ParserHelper::apply_declaration(props, property, value);
}

bool StyleParser::load_file(const std::string &path, StyleSheet &sheet) {
	return load_files(std::span(&path, 1), sheet);
}

bool StyleParser::load_files(std::span<const std::string> paths, StyleSheet &sheet) {
	sheet.clear();

	bool all_ok = true;
	for (const std::string &path : paths) {
		const std::string src = Platform::Filesystem::VirtualFileSystem::get()->read_text_file(path);
		if (src.empty()) {
			AQUILA_LOG_ERROR("StyleParser: cannot open '{}'", path);
			all_ok = false;
			continue;
		}
		if (!LoadString(src, sheet, false)) {
			all_ok = false;
		}
	}
	return all_ok;
}

bool StyleParser::LoadString(std::string_view css, StyleSheet &sheet, bool clear_first) {
	if (clear_first) {
		sheet.clear();
	}

	std::string src = ParserHelper::strip_comments(css);

	auto vars = extract_variables(src);
	for (const auto &[name, value] : vars) {
		sheet.add_variable(name, value);
	}

	const auto &all_vars = sheet.get_variables();
	if (!all_vars.empty()) {
		src = substitute_variables(std::move(src), all_vars);
	}

	size_t i = 0;
	while (i < src.size()) {
		while (i < src.size() && (std::isspace(static_cast<unsigned char>(src[i])) != 0)) {
			++i;
		}
		if (i >= src.size()) {
			break;
		}

		const size_t brace_open = src.find('{', i);
		if (brace_open == std::string::npos) {
			break;
		}

		const std::string selector = ParserHelper::trim(std::string_view(src).substr(i, brace_open - i));

		const size_t brace_close = find_matching_close(src, brace_open);
		if (brace_close == std::string::npos) {
			AQUILA_LOG_ERROR("StyleParser: unclosed block for selector '{}'", selector);
			return false;
		}

		const std::string_view body = std::string_view(src).substr(brace_open + 1, brace_close - brace_open - 1);

		if (!selector.empty()) {
			if (selector.size() > 6 && selector.starts_with("@media")) {
				parse_at_block(std::string_view(selector).substr(6), body, sheet, false);
			} else if (selector.size() > 10 && selector.substr(0, 10) == "@container") {
				parse_at_block(std::string_view(selector).substr(10), body, sheet, true);
			} else if (selector[0] != '@') {
				ParserHelper::parse_block(selector, body, sheet);
			}
		}

		i = brace_close + 1;
	}

	return true;
}

} // namespace Aquila::UI
