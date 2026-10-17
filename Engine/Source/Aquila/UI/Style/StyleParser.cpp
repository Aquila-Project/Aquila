#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Style/StyleSheet.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"

namespace Aquila::UI {

using Variables = std::unordered_map<std::string, std::string>;

static size_t find_matching_close(std::string_view src, size_t open_pos) {
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
	return depth == 0 ? i - 1 : std::string_view::npos;
}

static bool is_variable_name(std::string_view name) {
	return name.size() > 2 && name.starts_with("--") && std::ranges::all_of(name, [](char c) {
			   return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '_';
		   });
}

static bool starts_declaration(std::string_view src, size_t pos) {
	while (pos > 0 && std::isspace(static_cast<unsigned char>(src[pos - 1])) != 0) {
		--pos;
	}
	return pos == 0 || src[pos - 1] == '{' || src[pos - 1] == ';';
}

static Variables extract_variables(std::string_view src) {
	Variables vars;
	size_t i = 0;
	while (i < src.size()) {
		const size_t dash_pos = src.find("--", i);
		if (dash_pos == std::string_view::npos) {
			break;
		}

		const size_t colon_pos = src.find(':', dash_pos);
		const size_t end_pos = std::min(src.find(';', dash_pos), src.find('}', dash_pos));
		if (colon_pos == std::string_view::npos || end_pos == std::string_view::npos) {
			break;
		}

		std::string name = ParserHelper::trim(src.substr(dash_pos, colon_pos - dash_pos));
		if (colon_pos > end_pos || !is_variable_name(name) || !starts_declaration(src, dash_pos)) {
			i = dash_pos + 2;
			continue;
		}

		vars[std::move(name)] = ParserHelper::trim(src.substr(colon_pos + 1, end_pos - colon_pos - 1));
		i = end_pos;
	}
	return vars;
}

namespace {

class VariableResolver {
  public:
	explicit VariableResolver(const Variables &vars) : m_vars(vars) {}

	std::string resolve(std::string_view text, int depth = 0) {
		if (depth > k_max_depth) {
			AQUILA_LOG_WARNING("StyleParser: variables reference each other in a cycle near '{}'", text);
			return std::string(text);
		}

		std::string out;
		size_t i = 0;
		while (true) {
			const size_t start = text.find("var(", i);
			if (start == std::string_view::npos) {
				out.append(text.substr(i));
				return out;
			}

			size_t close = start + 4;
			size_t comma = std::string_view::npos;
			int nesting = 1;
			for (; close < text.size(); ++close) {
				if (text[close] == '(') {
					++nesting;
				} else if (text[close] == ')' && --nesting == 0) {
					break;
				} else if (text[close] == ',' && nesting == 1 && comma == std::string_view::npos) {
					comma = close;
				}
			}
			if (close >= text.size()) {
				AQUILA_LOG_WARNING("StyleParser: 'var(' is never closed near '{}'", text.substr(start, 32));
				out.append(text.substr(i));
				return out;
			}

			out.append(text.substr(i, start - i));
			const size_t name_end = comma == std::string_view::npos ? close : comma;
			const std::string name = ParserHelper::trim(text.substr(start + 4, name_end - start - 4));
			if (const auto it = m_vars.find(name); it != m_vars.end()) {
				out += resolve(it->second, depth + 1);
			} else if (comma != std::string_view::npos) {
				out += resolve(ParserHelper::trim_sv(text.substr(comma + 1, close - comma - 1)), depth + 1);
			} else {
				if (m_reported.insert(name).second) {
					AQUILA_LOG_WARNING("StyleParser: undefined variable '{}'", name);
				}
				out.append(text.substr(start, close - start + 1));
			}
			i = close + 1;
		}
	}

  private:
	static constexpr int k_max_depth = 16;

	const Variables &m_vars;
	std::unordered_set<std::string> m_reported;
};

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
			const std::string_view raw = std::string_view(cond_str).substr(j + 1, close_p - j - 1);
			if (auto cond = parse_media_condition(raw)) {
				conditions.push_back(*cond);
			} else {
				AQUILA_LOG_WARNING("StyleParser: unsupported condition '({})'", raw);
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

		const size_t inner_close = find_matching_close(body, inner_open);
		if (inner_close == std::string_view::npos) {
			AQUILA_LOG_ERROR("StyleParser: unclosed block for selector '{}'", inner_sel);
			break;
		}

		if (inner_sel.starts_with('@')) {
			AQUILA_LOG_WARNING("StyleParser: nested at-rules are not supported, skipping '{}'", inner_sel);
		} else if (!inner_sel.empty()) {
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

	src = VariableResolver(sheet.get_variables()).resolve(src);

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
			AQUILA_LOG_WARNING("StyleParser: ignoring '{}' at the end of the file, it has no '{{' block",
							   ParserHelper::trim(std::string_view(src).substr(i)));
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
			} else if (selector[0] == '@') {
				AQUILA_LOG_WARNING("StyleParser: unsupported at-rule '{}'", selector);
			} else {
				ParserHelper::parse_block(selector, body, sheet);
			}
		}

		i = brace_close + 1;
	}

	return true;
}

} // namespace Aquila::UI
