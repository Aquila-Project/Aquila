use std::{ffi::OsStr, fmt, fs, path::Path};

use crate::{
    lexer::Span,
    parser::{self, ParseErrorKind},
};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LayoutErrorKind {
    Style(ParseErrorKind),
    MissingRoot,
    ContentAfterRoot,
    TextOutsideRoot,
    ExpectedTagName,
    ExpectedAttributeName,
    ExpectedAttributeValue,
    UnterminatedQuote,
    UnquotedValueWithSlash,
    DuplicateAttribute,
    UnclosedTag,
    ExpectedTagEnd,
    UnclosedElement,
    MismatchedClosingTag { open: Span },
    UnexpectedClosingTag,
    UnterminatedComment,
    UnsupportedMarkup,
    IncludeWithoutSrc,
    IncludeNotSelfClosing,
    EntityNotDecoded,
    ImportWithoutSrc,
    ImportNotSelfClosing,
    ImportMissing,
    ComponentWithoutName,
    ComponentRootCount,
    SecondSlot,
    SlotAttribute,
    DuplicateComponent,
    SlotOutsideComponent,
    UndeclaredProp,
    ComponentDefinedTwice,
    MissingWindowStyle,
    ItemMisplaced { goes_in: &'static str },
    ItemAttribute,
    ItemContent,
    ComponentNamedLikeItem,
}

impl fmt::Display for LayoutErrorKind {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            LayoutErrorKind::Style(kind) => write!(f, "in `style`: {kind}"),
            LayoutErrorKind::MissingRoot => {
                f.write_str("no root element, a layout needs one (e.g. `<View>...</View>`)")
            }
            LayoutErrorKind::ContentAfterRoot => f.write_str(
                "content after the root element, the loader stops after the first element and ignores the rest",
            ),
            LayoutErrorKind::TextOutsideRoot => f.write_str("text outside the root element"),
            LayoutErrorKind::ExpectedTagName => f.write_str("expected a widget name after `<`"),
            LayoutErrorKind::ExpectedAttributeName => {
                f.write_str("expected an attribute name, `>` or `/>`")
            }
            LayoutErrorKind::ExpectedAttributeValue => f.write_str("expected a value after `=`"),
            LayoutErrorKind::UnterminatedQuote => f.write_str(
                "this quote is never closed, the loader reads the rest of the file as the value",
            ),
            LayoutErrorKind::UnquotedValueWithSlash => {
                f.write_str("an unquoted value stops at `/`, put it in quotes")
            }
            LayoutErrorKind::DuplicateAttribute => {
                f.write_str("attribute given twice, the last one wins")
            }
            LayoutErrorKind::UnclosedTag => f.write_str("this tag is never finished with `>` or `/>`"),
            LayoutErrorKind::ExpectedTagEnd => f.write_str("expected `>`"),
            LayoutErrorKind::UnclosedElement => f.write_str("this element is never closed"),
            LayoutErrorKind::MismatchedClosingTag { .. } => f.write_str(
                "closing tag doesn't match the open element, the loader closes the open one anyway",
            ),
            LayoutErrorKind::UnexpectedClosingTag => {
                f.write_str("closing tag without an open element")
            }
            LayoutErrorKind::UnterminatedComment => {
                f.write_str("this comment is never closed with `-->`")
            }
            LayoutErrorKind::UnsupportedMarkup => f.write_str(
                "only `<!-- -->` comments are supported, the loader gets stuck in an endless loop here",
            ),
            LayoutErrorKind::IncludeWithoutSrc => f.write_str("`<Include>` needs a `src`"),
            LayoutErrorKind::IncludeNotSelfClosing => f.write_str(
                "`<Include>` must be self-closing (`<Include src=\"...\"/>`), the loader treats what follows as siblings",
            ),
            LayoutErrorKind::EntityNotDecoded => {
                f.write_str("entities aren't decoded, this shows up literally")
            }
            LayoutErrorKind::ImportWithoutSrc => f.write_str("`<Import>` needs a `src`"),
            LayoutErrorKind::ImportNotSelfClosing => f.write_str(
                "`<Import>` must be self-closing (`<Import src=\"...\"/>`), its children are ignored",
            ),
            LayoutErrorKind::ImportMissing => f.write_str("the imported file doesn't exist"),
            LayoutErrorKind::ComponentWithoutName => f.write_str("`<Component>` needs a `name`"),
            LayoutErrorKind::ComponentRootCount => {
                f.write_str("a component needs exactly one root element, the loader drops it")
            }
            LayoutErrorKind::SecondSlot => {
                f.write_str("a component can only have one `<Slot/>`, the loader drops it")
            }
            LayoutErrorKind::SlotAttribute => {
                f.write_str("`<Slot/>` only takes `class`, the loader ignores this attribute")
            }
            LayoutErrorKind::DuplicateComponent => {
                f.write_str("a component with this name is already defined in this file")
            }
            LayoutErrorKind::SlotOutsideComponent => {
                f.write_str("`<Slot/>` only works inside a `<Component>`")
            }
            LayoutErrorKind::UndeclaredProp => f.write_str(
                "this prop isn't in the component's `props`, it becomes empty (`$$` writes a `$`)",
            ),
            LayoutErrorKind::ComponentDefinedTwice => f.write_str(
                "this import brings a component whose name this file already sees",
            ),
            LayoutErrorKind::MissingWindowStyle => {
                f.write_str("a window needs its .aqstyle next to its .aqlayout")
            }
            LayoutErrorKind::ItemMisplaced { goes_in } => {
                write!(f, "this item goes inside {goes_in}, the loader leaves it out")
            }
            LayoutErrorKind::ItemAttribute => {
                f.write_str("this item doesn't take this attribute, the loader ignores it")
            }
            LayoutErrorKind::ItemContent => f.write_str(
                "this item can't hold this, the loader leaves it out (labels go in an attribute)",
            ),
            LayoutErrorKind::ComponentNamedLikeItem => {
                f.write_str("a component can't have the name of an item tag")
            }
        }
    }
}

struct ItemRule {
    tag: &'static str,
    goes_in: &'static str,
    attributes: &'static [&'static str],
    items: &'static [&'static str],
    takes_elements: bool,
}

const MENU_ITEMS: &[&str] = &["MenuItem", "MenuSeparator", "Submenu"];
const IN_MENUS: &str = "a PopupMenu or a <Submenu>";

const ITEM_RULES: &[ItemRule] = &[
    ItemRule {
        tag: "Option",
        goes_in: "a Dropdown",
        attributes: &["value", "label", "icon"],
        items: &[],
        takes_elements: false,
    },
    ItemRule {
        tag: "Item",
        goes_in: "a ListBox",
        attributes: &["value", "label"],
        items: &[],
        takes_elements: false,
    },
    ItemRule {
        tag: "Tab",
        goes_in: "a TabView or a TabBar",
        attributes: &["title", "icon", "tooltip"],
        items: &[],
        takes_elements: true,
    },
    ItemRule {
        tag: "TreeNode",
        goes_in: "a TreeView or a <TreeNode>",
        attributes: &["label"],
        items: &["TreeNode"],
        takes_elements: false,
    },
    ItemRule {
        tag: "MenuItem",
        goes_in: IN_MENUS,
        attributes: &["command", "label", "icon"],
        items: &[],
        takes_elements: false,
    },
    ItemRule {
        tag: "MenuSeparator",
        goes_in: IN_MENUS,
        attributes: &[],
        items: &[],
        takes_elements: false,
    },
    ItemRule {
        tag: "Submenu",
        goes_in: IN_MENUS,
        attributes: &["label", "icon"],
        items: MENU_ITEMS,
        takes_elements: false,
    },
];

const ITEM_WIDGETS: &[(&str, &[&str])] = &[
    ("Dropdown", &["Option"]),
    ("ListBox", &["Item"]),
    ("TabView", &["Tab"]),
    ("TabBar", &["Tab"]),
    ("TreeView", &["TreeNode"]),
    ("PopupMenu", MENU_ITEMS),
];

fn item_rule(tag: &str) -> Option<&'static ItemRule> {
    ITEM_RULES.iter().find(|rule| rule.tag == tag)
}

fn takes_item(parent: Option<&str>, tag: &str) -> bool {
    let Some(parent) = parent else {
        return false;
    };
    if let Some(rule) = item_rule(parent) {
        return rule.items.contains(&tag);
    }
    ITEM_WIDGETS
        .iter()
        .find(|(widget, _)| *widget == parent)
        .map_or(true, |(_, items)| items.contains(&tag))
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct LayoutError {
    pub kind: LayoutErrorKind,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Attribute {
    pub name: Span,
    pub value: Option<Span>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Element {
    pub name: Span,
    pub attributes: Vec<Attribute>,
    pub children: Vec<Node>,
    pub self_closing: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Node {
    Element(Element),
    Text(Span),
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Layout {
    pub root: Option<Element>,
    pub imports: Vec<Element>,
    pub components: Vec<Element>,
}

pub fn parse(src: &str) -> (Layout, Vec<LayoutError>) {
    let mut parser = Parser {
        src,
        pos: 0,
        errors: Vec::new(),
        eof_reported: false,
    };
    let layout = parser.parse_document();
    (layout, parser.errors)
}

pub fn check_files(path: &Path, layout: &Layout, src: &str) -> Vec<LayoutError> {
    let text = |span: Span| &src[span.start..span.end];
    let mut errors = Vec::new();
    let mut seen: Vec<String> = layout
        .components
        .iter()
        .filter_map(|component| attribute_value(component, "name", src))
        .map(str::to_string)
        .collect();

    let dir = path.parent().unwrap_or(Path::new("."));
    for import in &layout.imports {
        let Some(value) = attribute_span(import, "src", src) else {
            continue;
        };
        let target = text(value);
        if target.is_empty() || target.starts_with('/') {
            continue;
        }
        let mut file = dir.join(target);
        if file.extension().is_none() {
            file.set_extension("aqlayout");
        }
        let Ok(imported_src) = fs::read_to_string(&file) else {
            errors.push(LayoutError {
                kind: LayoutErrorKind::ImportMissing,
                span: value,
            });
            continue;
        };
        let (imported, _) = parse(&imported_src);
        for component in &imported.components {
            if let Some(name) = attribute_value(component, "name", &imported_src) {
                if seen.iter().any(|existing| existing == name) {
                    errors.push(LayoutError {
                        kind: LayoutErrorKind::ComponentDefinedTwice,
                        span: value,
                    });
                } else {
                    seen.push(name.to_string());
                }
            }
        }
    }

    let in_windows = path.parent().and_then(Path::file_name) == Some(OsStr::new("Windows"));
    if in_windows && !path.with_extension("aqstyle").exists() {
        errors.push(LayoutError {
            kind: LayoutErrorKind::MissingWindowStyle,
            span: Span { start: 0, end: 0 },
        });
    }
    errors
}

fn attribute_span(element: &Element, name: &str, src: &str) -> Option<Span> {
    element
        .attributes
        .iter()
        .rev()
        .find(|attribute| &src[attribute.name.start..attribute.name.end] == name)
        .and_then(|attribute| attribute.value)
}

fn attribute_value<'a>(element: &Element, name: &str, src: &'a str) -> Option<&'a str> {
    let value = attribute_span(element, name, src)?;
    let text = &src[value.start..value.end];
    (!text.is_empty()).then_some(text)
}

fn has_prop_reference(text: &str) -> bool {
    let bytes = text.as_bytes();
    let mut pos = 0;
    while pos + 1 < bytes.len() {
        if bytes[pos] == b'$' {
            if bytes[pos + 1] == b'$' {
                pos += 2;
                continue;
            }
            if is_prop_start(bytes[pos + 1]) {
                return true;
            }
        }
        pos += 1;
    }
    false
}

fn is_prop_start(byte: u8) -> bool {
    byte.is_ascii_alphabetic() || byte == b'_'
}

fn is_prop_byte(byte: u8) -> bool {
    byte.is_ascii_alphanumeric() || matches!(byte, b'_' | b'-')
}

fn is_name_byte(byte: u8) -> bool {
    byte.is_ascii_alphanumeric() || matches!(byte, b'-' | b'_' | b':')
}

struct Parser<'src> {
    src: &'src str,
    pos: usize,
    errors: Vec<LayoutError>,
    eof_reported: bool,
}

impl<'src> Parser<'src> {
    fn peek(&self) -> Option<u8> {
        self.src.as_bytes().get(self.pos).copied()
    }

    fn peek_next(&self) -> Option<u8> {
        self.src.as_bytes().get(self.pos + 1).copied()
    }

    fn at(&self, text: &str) -> bool {
        self.src.as_bytes()[self.pos..].starts_with(text.as_bytes())
    }

    fn eat_while(&mut self, pred: impl Fn(u8) -> bool) -> Span {
        let start = self.pos;
        while self.peek().is_some_and(&pred) {
            self.pos += 1;
        }
        Span {
            start,
            end: self.pos,
        }
    }

    fn skip_whitespace(&mut self) {
        self.eat_while(|b| b.is_ascii_whitespace());
    }

    fn text(&self, span: Span) -> &'src str {
        &self.src[span.start..span.end]
    }

    fn here(&self) -> Span {
        let mut end = (self.pos + 1).min(self.src.len());
        while !self.src.is_char_boundary(end) {
            end += 1;
        }
        Span {
            start: self.pos,
            end,
        }
    }

    fn error(&mut self, kind: LayoutErrorKind, span: Span) {
        self.errors.push(LayoutError { kind, span });
    }

    fn eof_error(&mut self, kind: LayoutErrorKind, span: Span) {
        if !self.eof_reported {
            self.eof_reported = true;
            self.error(kind, span);
        }
    }

    fn skip_past_tag(&mut self) {
        while let Some(byte) = self.peek() {
            self.pos += 1;
            if byte == b'>' {
                break;
            }
        }
    }

    fn skip_tag_junk(&mut self) {
        while let Some(byte) = self.peek() {
            if byte == b'>' || (byte == b'/' && self.peek_next() == Some(b'>')) {
                break;
            }
            self.pos += 1;
        }
    }

    fn skip_markup(&mut self) {
        let start = self.pos;
        if self.at("<!--") {
            match self.src[start + 4..].find("-->") {
                Some(offset) => self.pos = start + 4 + offset + 3,
                None => {
                    self.pos = self.src.len();
                    self.eof_error(
                        LayoutErrorKind::UnterminatedComment,
                        Span {
                            start,
                            end: start + 4,
                        },
                    );
                }
            }
        } else {
            self.error(
                LayoutErrorKind::UnsupportedMarkup,
                Span {
                    start,
                    end: start + 2,
                },
            );
            self.skip_past_tag();
        }
    }

    fn parse_text(&mut self) -> Span {
        let span = self.eat_while(|b| b != b'<');
        Span {
            start: span.start,
            end: span.start + self.text(span).trim_end().len(),
        }
    }

    fn parse_document(&mut self) -> Layout {
        let mut layout = Layout {
            root: None,
            imports: Vec::new(),
            components: Vec::new(),
        };
        let mut saw_root = false;

        loop {
            self.skip_whitespace();
            match (self.peek(), self.peek_next()) {
                (None, _) => break,
                (Some(b'<'), Some(b'!')) => self.skip_markup(),
                (Some(b'<'), Some(b'?')) => self.skip_past_tag(),
                _ if saw_root => {
                    self.error(LayoutErrorKind::ContentAfterRoot, self.here());
                    break;
                }
                (Some(b'<'), Some(b'/')) => self.parse_closing_tag(None),
                (Some(b'<'), _) => match self.parse_element() {
                    Some(element) if self.text(element.name) == "Import" => {
                        self.check_import(&element);
                        layout.imports.push(element);
                    }
                    Some(element) if self.text(element.name) == "Component" => {
                        layout.components.push(element);
                    }
                    element => {
                        saw_root = true;
                        layout.root = element;
                    }
                },
                _ => {
                    let text = self.parse_text();
                    self.error(LayoutErrorKind::TextOutsideRoot, text);
                }
            }
        }

        if !saw_root && layout.imports.is_empty() && layout.components.is_empty() {
            self.error(LayoutErrorKind::MissingRoot, Span { start: 0, end: 0 });
        }
        self.check_components(&layout.components);
        if let Some(root) = &layout.root {
            self.check_no_slots(root);
            self.check_items(root, None);
        }
        for component in &layout.components {
            for child in &component.children {
                if let Node::Element(root) = child {
                    self.check_items(root, None);
                }
            }
        }
        layout
    }

    fn check_import(&mut self, import: &Element) {
        if attribute_value(import, "src", self.src).is_none() {
            self.error(LayoutErrorKind::ImportWithoutSrc, import.name);
        }
        if !import.self_closing {
            self.error(LayoutErrorKind::ImportNotSelfClosing, import.name);
        }
    }

    fn check_components(&mut self, components: &[Element]) {
        let mut names: Vec<&str> = Vec::new();
        for component in components {
            let name = attribute_value(component, "name", self.src);
            match name {
                None => self.error(LayoutErrorKind::ComponentWithoutName, component.name),
                Some(name) if names.contains(&name) => {
                    let span = attribute_span(component, "name", self.src).unwrap_or(component.name);
                    self.error(LayoutErrorKind::DuplicateComponent, span);
                }
                Some(name) => names.push(name),
            }
            if name.is_some_and(|name| item_rule(name).is_some()) {
                let span = attribute_span(component, "name", self.src).unwrap_or(component.name);
                self.error(LayoutErrorKind::ComponentNamedLikeItem, span);
            }

            let roots = component
                .children
                .iter()
                .filter(|child| matches!(child, Node::Element(_)))
                .count();
            if roots != 1 {
                self.error(LayoutErrorKind::ComponentRootCount, component.name);
            }

            let mut slots = Vec::new();
            for child in &component.children {
                self.collect_slots(child, &mut slots);
            }
            if let Some(&second) = slots.get(1) {
                self.error(LayoutErrorKind::SecondSlot, second);
            }
            for child in &component.children {
                self.check_slot_attributes(child);
            }

            let props: Vec<&str> = attribute_value(component, "props", self.src)
                .map(|value| value.split_whitespace().collect())
                .unwrap_or_default();
            for child in &component.children {
                self.check_props(child, &props);
            }
        }
    }

    fn collect_slots(&self, node: &Node, slots: &mut Vec<Span>) {
        if let Node::Element(element) = node {
            if self.text(element.name) == "Slot" {
                slots.push(element.name);
                return;
            }
            for child in &element.children {
                self.collect_slots(child, slots);
            }
        }
    }

    fn check_slot_attributes(&mut self, node: &Node) {
        if let Node::Element(element) = node {
            if self.text(element.name) == "Slot" {
                for attribute in &element.attributes {
                    if self.text(attribute.name) != "class" {
                        self.error(LayoutErrorKind::SlotAttribute, attribute.name);
                    }
                }
            }
            for child in &element.children {
                self.check_slot_attributes(child);
            }
        }
    }

    fn check_no_slots(&mut self, element: &Element) {
        let mut slots = Vec::new();
        self.collect_slots(&Node::Element(element.clone()), &mut slots);
        for slot in slots {
            self.error(LayoutErrorKind::SlotOutsideComponent, slot);
        }
    }

    fn check_items(&mut self, element: &Element, parent: Option<&str>) {
        let tag = self.text(element.name);
        let Some(rule) = item_rule(tag) else {
            for child in &element.children {
                if let Node::Element(child) = child {
                    self.check_items(child, Some(tag));
                }
            }
            return;
        };
        if !takes_item(parent, tag) {
            self.error(LayoutErrorKind::ItemMisplaced { goes_in: rule.goes_in }, element.name);
        }
        for attribute in &element.attributes {
            if !rule.attributes.contains(&self.text(attribute.name)) {
                self.error(LayoutErrorKind::ItemAttribute, attribute.name);
            }
        }
        for child in &element.children {
            match child {
                Node::Text(span) => self.error(LayoutErrorKind::ItemContent, *span),
                Node::Element(child) => {
                    let name = self.text(child.name);
                    if item_rule(name).is_none() && name != "Slot" && !rule.takes_elements {
                        self.error(LayoutErrorKind::ItemContent, child.name);
                    } else {
                        self.check_items(child, Some(tag));
                    }
                }
            }
        }
    }

    fn check_props(&mut self, node: &Node, props: &[&str]) {
        match node {
            Node::Text(span) => self.check_dollars(*span, props),
            Node::Element(element) => {
                for attribute in &element.attributes {
                    if let Some(value) = attribute.value {
                        self.check_dollars(value, props);
                    }
                }
                for child in &element.children {
                    self.check_props(child, props);
                }
            }
        }
    }

    fn check_dollars(&mut self, span: Span, props: &[&str]) {
        let bytes = self.src.as_bytes();
        let mut pos = span.start;
        while pos < span.end {
            if bytes[pos] != b'$' {
                pos += 1;
                continue;
            }
            if pos + 1 < span.end && bytes[pos + 1] == b'$' {
                pos += 2;
                continue;
            }
            if pos + 1 >= span.end || !is_prop_start(bytes[pos + 1]) {
                pos += 1;
                continue;
            }
            let end = (pos + 1..span.end)
                .find(|&i| !is_prop_byte(bytes[i]))
                .unwrap_or(span.end);
            if !props.contains(&&self.src[pos + 1..end]) {
                self.error(LayoutErrorKind::UndeclaredProp, Span { start: pos, end });
            }
            pos = end;
        }
    }

    fn parse_element(&mut self) -> Option<Element> {
        let open = self.pos;
        self.pos += 1;
        self.skip_whitespace();

        let name = self.eat_while(is_name_byte);
        if name.start == name.end {
            self.error(LayoutErrorKind::ExpectedTagName, self.here());
            self.skip_past_tag();
            return None;
        }

        let mut element = Element {
            name,
            attributes: Vec::new(),
            children: Vec::new(),
            self_closing: false,
        };

        let self_closing = loop {
            self.skip_whitespace();
            match (self.peek(), self.peek_next()) {
                (None, _) => {
                    self.eof_error(
                        LayoutErrorKind::UnclosedTag,
                        Span {
                            start: open,
                            end: name.end,
                        },
                    );
                    return Some(element);
                }
                (Some(b'>'), _) => {
                    self.pos += 1;
                    break false;
                }
                (Some(b'/'), Some(b'>')) => {
                    self.pos += 2;
                    element.self_closing = true;
                    break true;
                }
                (Some(byte), _) if is_name_byte(byte) => {
                    let attribute = self.parse_attribute();
                    self.add_attribute(&mut element, attribute);
                }
                _ => {
                    self.error(LayoutErrorKind::ExpectedAttributeName, self.here());
                    self.skip_tag_junk();
                }
            }
        };

        if self.text(name) == "Include" {
            let has_src = element.attributes.iter().any(|attribute| {
                self.text(attribute.name) == "src"
                    && attribute
                        .value
                        .is_some_and(|value| value.start != value.end)
            });
            if !has_src {
                self.error(LayoutErrorKind::IncludeWithoutSrc, name);
            }
            if !self_closing {
                self.error(LayoutErrorKind::IncludeNotSelfClosing, name);
            }
            return Some(element);
        }

        if self_closing {
            return Some(element);
        }

        loop {
            self.skip_whitespace();
            match (self.peek(), self.peek_next()) {
                (None, _) => {
                    self.eof_error(LayoutErrorKind::UnclosedElement, name);
                    break;
                }
                (Some(b'<'), Some(b'/')) => {
                    self.parse_closing_tag(Some(name));
                    break;
                }
                (Some(b'<'), Some(b'!')) => self.skip_markup(),
                (Some(b'<'), _) => {
                    if let Some(child) = self.parse_element() {
                        element.children.push(Node::Element(child));
                    }
                }
                _ => {
                    let text = self.parse_text();
                    self.check_entities(text);
                    element.children.push(Node::Text(text));
                }
            }
        }

        Some(element)
    }

    fn parse_closing_tag(&mut self, open: Option<Span>) {
        let start = self.pos;
        self.pos += 2;
        self.skip_whitespace();
        let name = self.eat_while(is_name_byte);
        self.skip_whitespace();

        match self.peek() {
            Some(b'>') => self.pos += 1,
            None => self.eof_error(
                LayoutErrorKind::UnclosedTag,
                Span {
                    start,
                    end: name.end,
                },
            ),
            Some(_) => {
                self.error(LayoutErrorKind::ExpectedTagEnd, self.here());
                self.skip_past_tag();
            }
        }

        let target = if name.start == name.end {
            Span {
                start,
                end: start + 2,
            }
        } else {
            name
        };

        match open {
            None => self.error(LayoutErrorKind::UnexpectedClosingTag, target),
            Some(open) if self.text(open) != self.text(name) => {
                self.error(LayoutErrorKind::MismatchedClosingTag { open }, target)
            }
            Some(_) => {}
        }
    }

    fn parse_attribute(&mut self) -> Attribute {
        let name = self.eat_while(is_name_byte);
        self.skip_whitespace();
        if self.peek() != Some(b'=') {
            return Attribute { name, value: None };
        }
        self.pos += 1;
        self.skip_whitespace();

        let value = match self.peek() {
            Some(quote @ (b'"' | b'\'')) => {
                let open = self.pos;
                self.pos += 1;
                let value = self.eat_while(|b| b != quote);
                if self.peek().is_some() {
                    self.pos += 1;
                } else {
                    self.eof_error(
                        LayoutErrorKind::UnterminatedQuote,
                        Span {
                            start: open,
                            end: open + 1,
                        },
                    );
                }
                value
            }
            _ => {
                let value = self.eat_while(|b| !b.is_ascii_whitespace() && b != b'>' && b != b'/');
                if value.start == value.end {
                    self.error(LayoutErrorKind::ExpectedAttributeValue, self.here());
                } else if self.peek() == Some(b'/') && self.peek_next() != Some(b'>') {
                    self.error(LayoutErrorKind::UnquotedValueWithSlash, value);
                    self.skip_tag_junk();
                }
                value
            }
        };

        Attribute {
            name,
            value: Some(value),
        }
    }

    fn add_attribute(&mut self, element: &mut Element, attribute: Attribute) {
        let name = self.text(attribute.name);
        if element
            .attributes
            .iter()
            .any(|existing| self.text(existing.name) == name)
        {
            self.error(LayoutErrorKind::DuplicateAttribute, attribute.name);
        }

        if let Some(value) = attribute.value {
            if name == "style" {
                self.check_inline_style(value);
            } else {
                self.check_entities(value);
            }
        }

        element.attributes.push(attribute);
    }

    fn check_inline_style(&mut self, value: Span) {
        if has_prop_reference(self.text(value)) {
            return;
        }
        let (_, errors) = parser::parse_inline(self.text(value));
        for error in errors {
            self.error(
                LayoutErrorKind::Style(error.kind),
                Span {
                    start: value.start + error.span.start,
                    end: value.start + error.span.end,
                },
            );
        }
    }

    fn check_entities(&mut self, span: Span) {
        let bytes = self.src.as_bytes();
        let mut pos = span.start;
        while pos < span.end {
            if bytes[pos] == b'&' {
                let name_end = (pos + 1..span.end)
                    .find(|&i| !(bytes[i].is_ascii_alphanumeric() || bytes[i] == b'#'))
                    .unwrap_or(span.end);
                if name_end > pos + 1 && name_end < span.end && bytes[name_end] == b';' {
                    self.error(
                        LayoutErrorKind::EntityNotDecoded,
                        Span {
                            start: pos,
                            end: name_end + 1,
                        },
                    );
                    pos = name_end + 1;
                    continue;
                }
            }
            pos += 1;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn text(src: &str, span: Span) -> &str {
        &src[span.start..span.end]
    }

    fn kinds(errors: &[LayoutError]) -> Vec<LayoutErrorKind> {
        errors.iter().map(|e| e.kind).collect()
    }

    fn attributes<'a>(src: &'a str, element: &Element) -> Vec<(&'a str, Option<&'a str>)> {
        element
            .attributes
            .iter()
            .map(|a| (text(src, a.name), a.value.map(|v| text(src, v))))
            .collect()
    }

    fn child(element: &Element, index: usize) -> &Element {
        match &element.children[index] {
            Node::Element(child) => child,
            Node::Text(_) => panic!("child {index} is text"),
        }
    }

    fn errors_of(src: &str) -> Vec<LayoutErrorKind> {
        kinds(&parse(src).1)
    }

    #[test]
    fn nested_elements_and_text() {
        let src = "<View id=\"root\">\n    <Label class=\"title\">Hello there</Label>\n    <Button/>\n</View>";
        let (layout, errors) = parse(src);
        assert!(errors.is_empty(), "{errors:?}");
        let root = layout.root.unwrap();
        assert_eq!(text(src, root.name), "View");
        assert_eq!(attributes(src, &root), [("id", Some("root"))]);
        let label = child(&root, 0);
        assert_eq!(text(src, label.name), "Label");
        let [Node::Text(content)] = label.children.as_slice() else {
            panic!("{:?}", label.children);
        };
        assert_eq!(text(src, *content), "Hello there");
        assert_eq!(text(src, child(&root, 1).name), "Button");
    }

    #[test]
    fn quoting_styles_and_bare_attributes() {
        let src = "<OverlayToolbar edge='left' along=8 hidden tooltip=\"Snap: 15°\"/>";
        let (layout, errors) = parse(src);
        assert!(errors.is_empty(), "{errors:?}");
        assert_eq!(
            attributes(src, &layout.root.unwrap()),
            [
                ("edge", Some("left")),
                ("along", Some("8")),
                ("hidden", None),
                ("tooltip", Some("Snap: 15°")),
            ]
        );
    }

    #[test]
    fn whitespace_comments_and_prolog() {
        let src = "<?xml version=\"1.0\"?>\n<!-- top -->\n< View >\n  <!-- inside -->\n</ View >\n<!-- after -->\n";
        let (layout, errors) = parse(src);
        assert!(errors.is_empty(), "{errors:?}");
        assert!(layout.root.is_some());
    }

    #[test]
    fn mismatched_closing_tag_points_at_both_names() {
        let src = "<View><Label>x</Labl></View>";
        let (_, errors) = parse(src);
        let [error] = errors.as_slice() else {
            panic!("{errors:?}");
        };
        assert_eq!(text(src, error.span), "Labl");
        let LayoutErrorKind::MismatchedClosingTag { open } = error.kind else {
            panic!("{errors:?}");
        };
        assert_eq!(text(src, open), "Label");
    }

    #[test]
    fn unclosed_element_is_reported_once() {
        let src = "<View><Button>";
        let (_, errors) = parse(src);
        assert_eq!(kinds(&errors), [LayoutErrorKind::UnclosedElement]);
        assert_eq!(text(src, errors[0].span), "Button");
    }

    #[test]
    fn unterminated_quote_is_reported_once() {
        assert_eq!(
            errors_of("<View><Button id=\"a/></View>"),
            [LayoutErrorKind::UnterminatedQuote]
        );
    }

    #[test]
    fn unquoted_value_with_a_slash() {
        let src = "<View><Image src=Engine/UI/x.svg/></View>";
        let (_, errors) = parse(src);
        assert_eq!(kinds(&errors), [LayoutErrorKind::UnquotedValueWithSlash]);
        assert_eq!(text(src, errors[0].span), "Engine");
    }

    #[test]
    fn duplicate_attribute() {
        assert_eq!(
            errors_of("<View id=\"a\" class=\"b\" id=\"c\"/>"),
            [LayoutErrorKind::DuplicateAttribute]
        );
    }

    #[test]
    fn doctype_would_hang_the_loader() {
        assert_eq!(
            errors_of("<!DOCTYPE layout>\n<View/>"),
            [LayoutErrorKind::UnsupportedMarkup]
        );
        assert_eq!(
            errors_of("<View><![CDATA[x]]></View>"),
            [LayoutErrorKind::UnsupportedMarkup]
        );
    }

    #[test]
    fn exactly_one_root() {
        assert_eq!(errors_of(""), [LayoutErrorKind::MissingRoot]);
        assert_eq!(
            errors_of("<!-- nothing -->"),
            [LayoutErrorKind::MissingRoot]
        );
        assert_eq!(
            errors_of("<View/>\n<View/>"),
            [LayoutErrorKind::ContentAfterRoot]
        );
        assert_eq!(
            errors_of("stray <View/>"),
            [LayoutErrorKind::TextOutsideRoot]
        );
        assert_eq!(
            errors_of("</View>"),
            [
                LayoutErrorKind::UnexpectedClosingTag,
                LayoutErrorKind::MissingRoot
            ]
        );
    }

    #[test]
    fn include_rules() {
        assert_eq!(errors_of("<View><Include src=\"a.aqlayout\"/></View>"), []);
        assert_eq!(
            errors_of("<View><Include/></View>"),
            [LayoutErrorKind::IncludeWithoutSrc]
        );
        assert_eq!(
            errors_of("<View><Include src=\"a.aqlayout\"></View>"),
            [LayoutErrorKind::IncludeNotSelfClosing]
        );
    }

    #[test]
    fn inline_style_errors_point_into_the_attribute() {
        let src = "<View style=\"gap: 2px; width 100%\"/>";
        let (_, errors) = parse(src);
        assert_eq!(
            kinds(&errors),
            [LayoutErrorKind::Style(ParseErrorKind::ExpectedColon)]
        );
        assert_eq!(text(src, errors[0].span), "100%");
        assert_eq!(errors_of("<View style=\"gap: 2px; width: 100%;\"/>"), []);
    }

    #[test]
    fn entities_are_not_decoded() {
        let src = "<Label tooltip=\"a &lt; b\">Drag &amp; drop &stuff</Label>";
        let (_, errors) = parse(src);
        assert_eq!(
            errors.iter().map(|e| text(src, e.span)).collect::<Vec<_>>(),
            ["&lt;", "&amp;"]
        );
    }

    #[test]
    fn imports_and_components_come_before_the_root() {
        assert_eq!(
            errors_of(r#"<Import src="card"/><Component name="A"><View/></Component><View><A/></View>"#),
            []
        );
        assert_eq!(errors_of(r#"<Component name="A"><View/></Component>"#), []);
        assert_eq!(
            errors_of(r#"<View/><Component name="A"><View/></Component>"#),
            [LayoutErrorKind::ContentAfterRoot]
        );
    }

    #[test]
    fn import_rules() {
        assert_eq!(
            errors_of("<Import/><View/>"),
            [LayoutErrorKind::ImportWithoutSrc]
        );
        assert_eq!(
            errors_of(r#"<Import src="a"></Import><View/>"#),
            [LayoutErrorKind::ImportNotSelfClosing]
        );
    }

    #[test]
    fn component_rules() {
        assert_eq!(
            errors_of("<Component><View/></Component>"),
            [LayoutErrorKind::ComponentWithoutName]
        );
        assert_eq!(
            errors_of(r#"<Component name="A"><View/><View/></Component>"#),
            [LayoutErrorKind::ComponentRootCount]
        );
        assert_eq!(
            errors_of(r#"<Component name="A"></Component>"#),
            [LayoutErrorKind::ComponentRootCount]
        );
        assert_eq!(
            errors_of(r#"<Component name="A"><View><Slot/><Slot/></View></Component>"#),
            [LayoutErrorKind::SecondSlot]
        );
        assert_eq!(
            errors_of(r#"<Component name="A"><View/></Component><Component name="A"><View/></Component>"#),
            [LayoutErrorKind::DuplicateComponent]
        );
    }

    #[test]
    fn slots_only_inside_components() {
        assert_eq!(
            errors_of("<View><Slot/></View>"),
            [LayoutErrorKind::SlotOutsideComponent]
        );
        assert_eq!(
            errors_of(r#"<Component name="A"><View><Slot/></View></Component>"#),
            []
        );
    }

    #[test]
    fn styles_with_props_are_checked_once_built() {
        assert_eq!(
            errors_of(r#"<Component name="A" props="w"><View style="width: $w"/></Component>"#),
            []
        );
        assert!(!errors_of(r#"<Component name="A"><View style="width: $$5 nope"/></Component>"#).is_empty());
    }

    #[test]
    fn slot_takes_a_class() {
        assert_eq!(
            errors_of(r#"<Component name="A"><View><Slot class="value"/></View></Component>"#),
            []
        );
    }

    #[test]
    fn slot_rejects_other_attributes() {
        let src = r#"<Component name="A"><View><Slot id="v" class="w"/></View></Component>"#;
        let (_, errors) = parse(src);
        assert_eq!(kinds(&errors), [LayoutErrorKind::SlotAttribute]);
        assert_eq!(&src[errors[0].span.start..errors[0].span.end], "id");
    }

    #[test]
    fn props_must_be_declared() {
        let src = r#"<Component name="A" props="title"><View tooltip="$title $$5 $5"><Label>$nope</Label></View></Component>"#;
        let (_, errors) = parse(src);
        assert_eq!(kinds(&errors), [LayoutErrorKind::UndeclaredProp]);
        assert_eq!(text(src, errors[0].span), "$nope");
        assert_eq!(errors_of(r#"<View tooltip="$anything"/>"#), []);
    }

    #[test]
    fn items_inside_widgets_that_take_them() {
        let src = r#"<View>
    <Dropdown><Option value="a" label="A" icon="a.svg"/></Dropdown>
    <ListBox><Item value="a" label="Alpha"/></ListBox>
    <TabView><Tab title="One" tooltip="first"><Label>page</Label></Tab></TabView>
    <TabBar><Tab title="Recent" icon="clock.svg"/></TabBar>
    <TreeView><TreeNode label="Root"><TreeNode label="Child"/></TreeNode></TreeView>
    <PopupMenu>
        <MenuItem command="scene.save" icon="save.svg"/>
        <MenuSeparator/>
        <Submenu label="More"><MenuItem label="Inner"/><Submenu label="Deeper"/></Submenu>
    </PopupMenu>
</View>"#;
        assert_eq!(errors_of(src), []);
    }

    #[test]
    fn items_in_the_wrong_place() {
        let src = r#"<View><Option value="a"/></View>"#;
        assert_eq!(errors_of(src), []);
        let (_, errors) = parse(r#"<MenuItem label="x"/>"#);
        assert_eq!(kinds(&errors), [LayoutErrorKind::ItemMisplaced { goes_in: "a PopupMenu or a <Submenu>" }]);
        assert_eq!(
            errors_of(r#"<Component name="A"><Tab title="x"/></Component>"#),
            [LayoutErrorKind::ItemMisplaced { goes_in: "a TabView or a TabBar" }]
        );
        let src = r#"<TabBar><Option value="a"/></TabBar>"#;
        let (_, errors) = parse(src);
        assert_eq!(kinds(&errors), [LayoutErrorKind::ItemMisplaced { goes_in: "a Dropdown" }]);
        assert_eq!(text(src, errors[0].span), "Option");
        assert_eq!(
            errors_of(r#"<PopupMenu><Submenu label="x"><TreeNode label="y"/></Submenu></PopupMenu>"#),
            [LayoutErrorKind::ItemMisplaced { goes_in: "a TreeView or a <TreeNode>" }]
        );
        assert_eq!(
            errors_of(r#"<TabView><Tab title="x"><Tab title="y"/></Tab></TabView>"#),
            [LayoutErrorKind::ItemMisplaced { goes_in: "a TabView or a TabBar" }]
        );
    }

    #[test]
    fn item_attributes_are_checked() {
        let src = r#"<Dropdown><Option value="a" lable="A"/></Dropdown>"#;
        let (_, errors) = parse(src);
        assert_eq!(kinds(&errors), [LayoutErrorKind::ItemAttribute]);
        assert_eq!(text(src, errors[0].span), "lable");
        assert_eq!(
            errors_of(r#"<PopupMenu><MenuSeparator id="s"/></PopupMenu>"#),
            [LayoutErrorKind::ItemAttribute]
        );
    }

    #[test]
    fn item_contents_are_checked() {
        assert_eq!(
            errors_of(r#"<Dropdown><Option value="a">A</Option></Dropdown>"#),
            [LayoutErrorKind::ItemContent]
        );
        assert_eq!(
            errors_of(r#"<PopupMenu><Submenu label="x"><Label>no</Label></Submenu></PopupMenu>"#),
            [LayoutErrorKind::ItemContent]
        );
        assert_eq!(
            errors_of(r#"<Component name="A"><PopupMenu><Submenu label="x"><Slot/></Submenu></PopupMenu></Component>"#),
            []
        );
    }

    #[test]
    fn components_cant_take_item_names() {
        let src = r#"<Component name="Tab"><View/></Component>"#;
        let (_, errors) = parse(src);
        assert_eq!(kinds(&errors), [LayoutErrorKind::ComponentNamedLikeItem]);
        assert_eq!(text(src, errors[0].span), "Tab");
    }

    fn scratch_dir(name: &str) -> std::path::PathBuf {
        let dir = std::env::temp_dir().join(format!("aqlint-{name}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(dir.join("Windows")).unwrap();
        std::fs::create_dir_all(dir.join("Components")).unwrap();
        dir
    }

    fn file_errors_of(path: &std::path::Path) -> Vec<LayoutErrorKind> {
        let src = std::fs::read_to_string(path).unwrap();
        let (layout, _) = parse(&src);
        kinds(&check_files(path, &layout, &src))
    }

    #[test]
    fn imports_must_exist() {
        let dir = scratch_dir("imports");
        std::fs::write(dir.join("Components/card.aqlayout"), r#"<Component name="Card"><View/></Component>"#).unwrap();
        let ok = dir.join("Windows/ok.aqlayout");
        std::fs::write(&ok, r#"<Import src="../Components/card"/><View/>"#).unwrap();
        std::fs::write(dir.join("Windows/ok.aqstyle"), "").unwrap();
        assert_eq!(file_errors_of(&ok), []);
        let missing = dir.join("Components/missing.aqlayout");
        std::fs::write(&missing, r#"<Import src="nowhere"/><Component name="M"><View/></Component>"#).unwrap();
        assert_eq!(file_errors_of(&missing), [LayoutErrorKind::ImportMissing]);
    }

    #[test]
    fn a_name_from_an_import_is_not_defined_again() {
        let dir = scratch_dir("twice");
        std::fs::write(dir.join("Components/card.aqlayout"), r#"<Component name="Card"><View/></Component>"#).unwrap();
        let twice = dir.join("Components/twice.aqlayout");
        std::fs::write(&twice, r#"<Import src="card"/><Component name="Card"><View/></Component>"#).unwrap();
        assert_eq!(file_errors_of(&twice), [LayoutErrorKind::ComponentDefinedTwice]);
    }

    #[test]
    fn a_window_needs_its_style() {
        let dir = scratch_dir("windows");
        let window = dir.join("Windows/w.aqlayout");
        std::fs::write(&window, "<View/>").unwrap();
        assert_eq!(file_errors_of(&window), [LayoutErrorKind::MissingWindowStyle]);
        std::fs::write(dir.join("Windows/w.aqstyle"), "").unwrap();
        assert_eq!(file_errors_of(&window), []);
    }

    #[test]
    fn stray_input_reports_errors_and_terminates() {
        for src in [
            "<",
            "</",
            "<>",
            "<View",
            "<View id",
            "<View id=",
            "<View id=\"",
            "<View/",
            "<!",
            "<!--",
            "<?",
            "<View><",
            "<View></",
            "<View><!-",
            "<View><?x?></View>",
            ">",
            "/>",
            "<View \"x\"/>",
        ] {
            assert!(!errors_of(src).is_empty(), "{src:?}");
        }
    }
}
