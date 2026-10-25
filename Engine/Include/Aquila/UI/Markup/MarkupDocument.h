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

struct MarkupDocument {
	std::string source_name;
	Option<MarkupNode> root;

	[[nodiscard]] std::string location(SourceLocation where) const;
};

}

#endif
