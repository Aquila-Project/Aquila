#ifndef AQUILA_UI_MARKUP_MARKUP_DOCUMENT_H
#define AQUILA_UI_MARKUP_MARKUP_DOCUMENT_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::UI::Markup {

struct SourceLocation {
	Uint32 line = 0;
	Uint32 column = 0;
};

struct MarkupAttribute {
	std::string name;
	std::string value;
	SourceLocation where;
	SourceLocation value_where;
};

struct MarkupNode {
	std::string tag;
	std::string text;
	std::vector<MarkupAttribute> attributes;
	std::vector<MarkupNode> children;
	bool self_closing = false;
	SourceLocation where;

	[[nodiscard]] bool is_text() const { return tag.empty(); }
	[[nodiscard]] const MarkupAttribute *find(std::string_view name) const;
};

struct ComponentDefinition {
	std::string name;
	std::string behaviour;
	std::vector<std::string> props;
	MarkupNode root;
	bool has_slot = false;
	SourceLocation where;
};

struct MarkupImport {
	std::string src;
	SourceLocation where;
};

struct MarkupDocument {
	std::string source_name;
	std::vector<MarkupImport> imports;
	std::vector<ComponentDefinition> components;
	Option<MarkupNode> root;

	[[nodiscard]] const ComponentDefinition *find_component(std::string_view name) const;

	[[nodiscard]] std::string location(SourceLocation where) const;
};

}

#endif
