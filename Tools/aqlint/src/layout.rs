use std::fmt;

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
        }
    }
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
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Node {
    Element(Element),
    Text(Span),
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Layout {
    pub root: Option<Element>,
}

pub fn parse(src: &str) -> (Layout, Vec<LayoutError>) {
    let mut parser = Parser {
        src,
        pos: 0,
        errors: Vec::new(),
        eof_reported: false,
    };
    let root = parser.parse_document();
    (Layout { root }, parser.errors)
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

    fn parse_document(&mut self) -> Option<Element> {
        let mut root = None;
        let mut saw_element = false;

        loop {
            self.skip_whitespace();
            match (self.peek(), self.peek_next()) {
                (None, _) => break,
                (Some(b'<'), Some(b'!')) => self.skip_markup(),
                (Some(b'<'), Some(b'?')) => self.skip_past_tag(),
                _ if saw_element => {
                    self.error(LayoutErrorKind::ContentAfterRoot, self.here());
                    break;
                }
                (Some(b'<'), Some(b'/')) => self.parse_closing_tag(None),
                (Some(b'<'), _) => {
                    saw_element = true;
                    root = self.parse_element();
                }
                _ => {
                    let text = self.parse_text();
                    self.error(LayoutErrorKind::TextOutsideRoot, text);
                }
            }
        }

        if !saw_element {
            self.error(LayoutErrorKind::MissingRoot, Span { start: 0, end: 0 });
        }
        root
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
