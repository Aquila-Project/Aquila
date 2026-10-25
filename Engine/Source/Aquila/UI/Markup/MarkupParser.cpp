#include "Aquila/UI/Markup/MarkupParser.h"

#include "Aquila/Foundation/Log.h"
#include "Aquila/Foundation/Macros.h"

namespace Aquila::UI::Markup {

namespace {

struct Parser {
	std::string_view src;
	MarkupDocument &document;
	std::vector<size_t> line_starts;
	size_t pos = 0;

	Parser(std::string_view text, MarkupDocument &doc) : src(text), document(doc) {
		line_starts.push_back(0);
		for (size_t i = 0; i < src.size(); ++i) {
			if (src[i] == '\n') {
				line_starts.push_back(i + 1);
			}
		}
	}

	[[nodiscard]] bool at_end() const { return pos >= src.size(); }
	[[nodiscard]] char peek() const { return at_end() ? '\0' : src[pos]; }
	[[nodiscard]] char peek2() const { return (pos + 1 < src.size()) ? src[pos + 1] : '\0'; }
	[[nodiscard]] bool at(std::string_view text) const { return src.substr(pos).starts_with(text); }
	[[nodiscard]] bool at_tag_end() const { return peek() == '>' || (peek() == '/' && peek2() == '>'); }

	char advance() {
		AQUILA_ASSERT(!at_end(), "LayoutLoader: unexpected end of input");
		return src[pos++];
	}

	[[nodiscard]] SourceLocation location_of(size_t offset) const {
		offset = std::min(offset, src.size());
		const auto line = std::ranges::upper_bound(line_starts, offset) - line_starts.begin();
		const size_t line_start = line_starts[static_cast<size_t>(line) - 1];
		return { .line = static_cast<Uint32>(line), .column = static_cast<Uint32>(offset - line_start + 1) };
	}

	[[nodiscard]] std::string location(size_t offset) const { return document.location(location_of(offset)); }

	void skip_ws() {
		while (!at_end() && (std::isspace(static_cast<unsigned char>(peek())) != 0)) {
			advance();
		}
	}

	void skip_past(char c) {
		while (!at_end() && advance() != c) {
		}
	}

	void skip_tag_junk() {
		while (!at_end() && !at_tag_end()) {
			advance();
		}
	}

	bool match(char c) {
		if (peek() == c) {
			advance();
			return true;
		}
		return false;
	}

	std::string read_name() {
		size_t start = pos;
		while (!at_end() &&
			   ((std::isalnum(static_cast<unsigned char>(peek())) != 0) || peek() == '-' || peek() == '_' ||
				peek() == ':')) {
			advance();
		}
		return std::string(src.substr(start, pos - start));
	}

	std::string read_quoted() {
		const size_t open = pos;
		char q = advance();
		size_t start = pos;
		while (!at_end() && peek() != q) {
			advance();
		}
		std::string val(src.substr(start, pos - start));
		if (at_end()) {
			AQUILA_LOG_ERROR("LayoutLoader: {}: this quote is never closed", location(open));
		} else {
			advance();
		}
		return val;
	}

	std::string read_unquoted() {
		size_t start = pos;
		while (!at_end() && peek() != '>' && peek() != '/' && (std::isspace(static_cast<unsigned char>(peek())) == 0)) {
			advance();
		}
		std::string value(src.substr(start, pos - start));
		if (peek() == '/' && peek2() != '>') {
			AQUILA_LOG_ERROR("LayoutLoader: {}: the unquoted value '{}' stops at '/', put it in quotes",
							 location(start), value);
			skip_tag_junk();
		}
		return value;
	}

	std::string read_text_content() {
		size_t start = pos;
		while (!at_end() && peek() != '<') {
			advance();
		}
		std::string_view raw = src.substr(start, pos - start);
		while (!raw.empty() && (std::isspace(static_cast<unsigned char>(raw.front())) != 0)) {
			raw.remove_prefix(1);
		}
		while (!raw.empty() && (std::isspace(static_cast<unsigned char>(raw.back())) != 0)) {
			raw.remove_suffix(1);
		}
		return std::string(raw);
	}

	void skip_markup() {
		const size_t start = pos;
		if (at("<!--")) {
			const size_t close = src.find("-->", pos + 4);
			if (close == std::string_view::npos) {
				AQUILA_LOG_ERROR("LayoutLoader: {}: this comment is never closed with '-->'", location(start));
				pos = src.size();
				return;
			}
			pos = close + 3;
			return;
		}
		AQUILA_LOG_ERROR("LayoutLoader: {}: only <!-- --> comments are supported here", location(start));
		skip_past('>');
	}

	void parse_closing_tag(std::string_view open_name) {
		const size_t start = pos;
		pos += 2;
		skip_ws();
		const std::string name = read_name();
		skip_ws();
		if (!match('>')) {
			AQUILA_LOG_ERROR("LayoutLoader: {}: expected '>' to end </{}>", location(pos), name);
			skip_past('>');
		}
		if (name != open_name) {
			AQUILA_LOG_ERROR("LayoutLoader: {}: </{}> closes <{}>", location(start), name, open_name);
		}
	}

	void parse_children(MarkupNode &parent, size_t tag_start) {
		while (true) {
			skip_ws();
			if (at_end()) {
				AQUILA_LOG_ERROR("LayoutLoader: {}: <{}> is never closed", location(tag_start), parent.tag);
				return;
			}
			if (peek() == '<' && peek2() == '/') {
				parse_closing_tag(parent.tag);
				return;
			}
			if (peek() == '<' && peek2() == '!') {
				skip_markup();
				continue;
			}

			if (peek() == '<') {
				if (Option<MarkupNode> child = parse_element()) {
					parent.children.push_back(std::move(*child));
				}
				continue;
			}

			MarkupNode text;
			text.where = location_of(pos);
			text.text = read_text_content();
			parent.children.push_back(std::move(text));
		}
	}

	Option<MarkupNode> parse_element() {
		const size_t tag_start = pos;
		advance();
		skip_ws();

		MarkupNode node;
		node.where = location_of(tag_start);
		node.tag = read_name();
		if (node.tag.empty()) {
			AQUILA_LOG_ERROR("LayoutLoader: {}: expected a widget name after '<'", location(tag_start));
			skip_past('>');
			return std::nullopt;
		}

		skip_ws();
		while (!at_end() && !at_tag_end()) {
			const size_t attr_start = pos;
			MarkupAttribute attribute;
			attribute.where = location_of(attr_start);
			attribute.name = read_name();
			if (attribute.name.empty()) {
				AQUILA_LOG_ERROR("LayoutLoader: {}: expected an attribute name, '>' or '/>' in <{}>",
								 location(attr_start), node.tag);
				advance();
				skip_tag_junk();
				break;
			}
			const bool seen = std::ranges::any_of(
				node.attributes, [&attribute](const MarkupAttribute &other) { return other.name == attribute.name; });
			if (seen) {
				AQUILA_LOG_WARNING("LayoutLoader: {}: attribute '{}' is given twice", location(attr_start),
								   attribute.name);
			}

			skip_ws();
			attribute.value_where = location_of(pos);
			if (match('=')) {
				skip_ws();
				attribute.value_where = location_of(pos);
				attribute.value = (peek() == '"' || peek() == '\'') ? read_quoted() : read_unquoted();
			}
			skip_ws();
			node.attributes.push_back(std::move(attribute));
		}

		node.self_closing = match('/');
		if (!match('>')) {
			AQUILA_LOG_ERROR("LayoutLoader: {}: <{}> is never finished with '>' or '/>'", location(tag_start),
							 node.tag);
		}

		if (!node.self_closing) {
			parse_children(node, tag_start);
		}
		return node;
	}

	void skip_prolog() {
		if (at("\xEF\xBB\xBF")) {
			pos += 3;
		}
	}

	void add_definition(MarkupNode node) {
		const std::string where = document.location(node.where);
		const MarkupAttribute *name = node.find("name");
		if (name == nullptr || name->value.empty()) {
			AQUILA_LOG_ERROR("LayoutLoader: {}: a <Component> needs a 'name'", where);
			return;
		}

		ComponentDefinition definition;
		definition.name = name->value;
		definition.where = node.where;
		if (const MarkupAttribute *behaviour = node.find("behaviour")) {
			definition.behaviour = behaviour->value;
		}
		if (const MarkupAttribute *props = node.find("props")) {
			std::istringstream stream(props->value);
			std::string prop;
			while (stream >> prop) {
				definition.props.push_back(prop);
			}
		}

		std::vector<MarkupNode *> elements;
		for (MarkupNode &child : node.children) {
			if (!child.is_text()) {
				elements.push_back(&child);
			}
		}
		if (elements.size() != 1) {
			AQUILA_LOG_ERROR("LayoutLoader: {}: <Component '{}'> needs exactly one root element", where,
							 definition.name);
			return;
		}
		definition.root = std::move(*elements.front());

		const Usize slots = count_slots(definition.root);
		if (slots > 1) {
			AQUILA_LOG_ERROR("LayoutLoader: {}: <Component '{}'> can only have one <Slot/>", where, definition.name);
			return;
		}
		definition.has_slot = slots == 1;

		if (document.find_component(definition.name) != nullptr) {
			AQUILA_LOG_ERROR("LayoutLoader: {}: a second <Component> named '{}'", where, definition.name);
			return;
		}
		document.components.push_back(std::move(definition));
	}

	static Usize count_slots(const MarkupNode &node) {
		if (node.tag == "Slot") {
			return 1;
		}
		Usize count = 0;
		for (const MarkupNode &child : node.children) {
			count += count_slots(child);
		}
		return count;
	}

	void add_import(const MarkupNode &node) {
		const std::string where = document.location(node.where);
		if (!node.children.empty()) {
			AQUILA_LOG_WARNING("LayoutLoader: {}: the children of <Import> are ignored", where);
		}
		const MarkupAttribute *src = node.find("src");
		if (src == nullptr || src->value.empty()) {
			AQUILA_LOG_ERROR("LayoutLoader: {}: <Import> is missing a 'src' attribute", where);
			return;
		}
		document.imports.push_back({ .src = src->value, .where = node.where });
	}

	void parse() {
		skip_prolog();
		bool saw_definition = false;
		while (true) {
			skip_ws();
			if (at_end()) {
				if (!saw_definition) {
					AQUILA_LOG_ERROR("LayoutLoader: {}: no root element", document.source_name);
				}
				return;
			}
			if (peek() == '<' && peek2() == '!') {
				skip_markup();
				continue;
			}
			if (peek() == '<' && peek2() == '?') {
				skip_past('>');
				continue;
			}
			if (peek() == '<' && peek2() != '/') {
				Option<MarkupNode> element = parse_element();
				if (element && element->tag == "Component") {
					saw_definition = true;
					add_definition(std::move(*element));
					continue;
				}
				if (element && element->tag == "Import") {
					saw_definition = true;
					add_import(*element);
					continue;
				}
				document.root = std::move(element);
				break;
			}
			AQUILA_LOG_ERROR("LayoutLoader: {}: unexpected content before the root element", location(pos));
			if (peek() == '<') {
				skip_past('>');
			} else {
				read_text_content();
			}
		}

		while (true) {
			skip_ws();
			if (at_end()) {
				break;
			}
			if (peek() == '<' && peek2() == '!') {
				skip_markup();
				continue;
			}
			AQUILA_LOG_WARNING("LayoutLoader: {}: content after the root element is ignored", location(pos));
			break;
		}
	}
};

}

const MarkupAttribute *MarkupNode::find(std::string_view name) const {
	for (auto it = attributes.rbegin(); it != attributes.rend(); ++it) {
		if (it->name == name) {
			return &*it;
		}
	}
	return nullptr;
}

const ComponentDefinition *MarkupDocument::find_component(std::string_view name) const {
	const auto it = std::ranges::find(components, name, &ComponentDefinition::name);
	return it != components.end() ? &*it : nullptr;
}

std::string MarkupDocument::location(SourceLocation where) const {
	return source_name + ":" + std::to_string(where.line) + ":" + std::to_string(where.column);
}

MarkupDocument parse_markup(std::string_view text, std::string source_name) {
	MarkupDocument document;
	document.source_name = std::move(source_name);
	Parser parser(text, document);
	parser.parse();
	return document;
}

}
