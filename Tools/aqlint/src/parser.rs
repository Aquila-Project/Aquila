use std::fmt;

use crate::{
    lexer::{LexError, Lexer, Span, Token, TokenKind},
    parser::ParseErrorKind::Lex,
};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ParseErrorKind {
    Lex(LexError),
    ExpectedSelector,
    SpaceInSelector,
    ExpectedLeftBrace,
    ExpectedPropertyName,
    ExpectedColon,
    ExpectedValue,
    ExpectedLeftParen,
    ExpectedRightParen,
    UnclosedBlock,
}

impl fmt::Display for ParseErrorKind {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Lex(error) => write!(f, "{error}"),
            ParseErrorKind::ExpectedSelector => {
                f.write_str("expected a selector (`.class`, `#id`, `Type` or `:state`)")
            }
            ParseErrorKind::SpaceInSelector => f.write_str("unexpected space inside a selector"),
            ParseErrorKind::ExpectedLeftBrace => f.write_str("expected `{`"),
            ParseErrorKind::ExpectedPropertyName => f.write_str("expected a property name"),
            ParseErrorKind::ExpectedColon => f.write_str("expected `:` after the property name"),
            ParseErrorKind::ExpectedValue => f.write_str("expected a value"),
            ParseErrorKind::ExpectedLeftParen => f.write_str("expected `(` after the at-rule name"),
            ParseErrorKind::ExpectedRightParen => {
                f.write_str("expected `)` to close the condition")
            }
            ParseErrorKind::UnclosedBlock => f.write_str("this `{` is never closed"),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ParseError {
    pub kind: ParseErrorKind,
    pub span: Span,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SelectorKind {
    Class,
    Id,
    Type,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Selector {
    pub base: Option<(SelectorKind, Span)>,
    pub pseudo: Option<Span>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Declaration {
    pub name: Span,
    pub value: Vec<Token>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Rule {
    pub selectors: Vec<Selector>,
    pub declarations: Vec<Declaration>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AtRule {
    pub name: Span,
    pub condition: Vec<Token>,
    pub rules: Vec<Rule>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Item {
    Rule(Rule),
    AtRule(AtRule),
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Stylesheet {
    pub items: Vec<Item>,
}

pub fn parse(src: &str) -> (Stylesheet, Vec<ParseError>) {
    let mut parser = Parser::new(src);
    let stylesheet = parser.parse_stylesheet();
    (stylesheet, parser.errors)
}

pub fn parse_inline(src: &str) -> (Vec<Declaration>, Vec<ParseError>) {
    let mut parser = Parser::new(src);
    let declarations = parser.parse_inline_declarations();
    (declarations, parser.errors)
}

fn significant_tokens(src: &str, errors: &mut Vec<ParseError>) -> Vec<Token> {
    let mut tokens: Vec<Token> = Vec::new();

    for token in Lexer::new(src) {
        match &token.kind {
            TokenKind::Error(lex_error) => {
                errors.push(ParseError {
                    kind: Lex(*lex_error),
                    span: token.span,
                });
                continue;
            }
            TokenKind::Comment => {
                continue;
            }
            _ => {}
        }

        tokens.push(token);
    }

    tokens
}

struct Parser {
    tokens: Vec<Token>,
    pos: usize,
    end: usize,
    errors: Vec<ParseError>,
}

impl Parser {
    fn new(src: &str) -> Self {
        let mut errors = Vec::new();
        let tokens = significant_tokens(src, &mut errors);
        Parser {
            tokens,
            pos: 0,
            end: src.len(),
            errors,
        }
    }

    fn peek(&self) -> Option<Token> {
        self.tokens.get(self.pos).copied()
    }

    fn at(&self, kind: TokenKind) -> bool {
        self.peek().is_some_and(|t| t.kind == kind)
    }

    fn advance(&mut self) -> Option<Token> {
        let token = self.peek()?;
        self.pos += 1;
        Some(token)
    }

    fn eat(&mut self, kind: TokenKind) -> Option<Token> {
        if self.at(kind) { self.advance() } else { None }
    }

    fn span_here(&self) -> Span {
        self.peek().map_or(
            Span {
                start: self.end,
                end: self.end,
            },
            |t| t.span,
        )
    }

    fn error(&mut self, kind: ParseErrorKind, span: Span) {
        self.errors.push(ParseError { kind, span });
    }

    fn check_touching(&mut self, before: Span, after: Span) {
        if before.end != after.start {
            self.error(
                ParseErrorKind::SpaceInSelector,
                Span {
                    start: before.end,
                    end: after.start,
                },
            );
        }
    }

    fn skip_past_block(&mut self) {
        // todo!(
        // "consume tokens until a `}}` has been consumed or the input ends; always consume at least one token so the callers' loops move forward"
        // )

        loop {
            match self.advance() {
                // check whether its }
                Some(token) => {
                    if token.kind == TokenKind::RightBrace {
                        break;
                    }
                }
                None => break,
            }
        }
    }

    fn recover_declaration(&mut self) {
        loop {
            if self.at(TokenKind::RightBrace) {
                break;
            }

            match self.advance() {
                Some(token) => {
                    if token.kind == TokenKind::Semicolon {
                        break;
                    }
                }
                None => break,
            }
        }
    }

    fn parse_stylesheet(&mut self) -> Stylesheet {
        let mut items = Vec::new();
        while let Some(token) = self.peek() {
            let item = match token.kind {
                TokenKind::AtKeyword => self.parse_at_rule().map(Item::AtRule),
                _ => self.parse_rule().map(Item::Rule),
            };
            if let Some(item) = item {
                items.push(item);
            }
        }
        Stylesheet { items }
    }

    fn parse_at_rule(&mut self) -> Option<AtRule> {
        let name = self.advance()?;

        if self.eat(TokenKind::LeftParen).is_none() {
            self.error(ParseErrorKind::ExpectedLeftParen, self.span_here());
            self.skip_past_block();
            return None;
        }

        let mut condition = Vec::new();
        while let Some(token) = self.peek() {
            if matches!(
                token.kind,
                TokenKind::RightParen | TokenKind::LeftBrace | TokenKind::RightBrace
            ) {
                break;
            }
            condition.push(token);
            self.advance();
        }

        if self.eat(TokenKind::RightParen).is_none() {
            self.error(ParseErrorKind::ExpectedRightParen, self.span_here());
            self.skip_past_block();
            return None;
        }

        let Some(open) = self.eat(TokenKind::LeftBrace) else {
            self.error(ParseErrorKind::ExpectedLeftBrace, self.span_here());
            self.skip_past_block();
            return None;
        };

        let mut rules = Vec::new();
        loop {
            if self.eat(TokenKind::RightBrace).is_some() {
                break;
            }

            if self.peek().is_none() {
                self.error(ParseErrorKind::UnclosedBlock, open.span);
                break;
            }

            if let Some(rule) = self.parse_rule() {
                rules.push(rule);
            }
        }

        Some(AtRule {
            name: name.span,
            condition,
            rules,
        })
    }

    fn parse_rule(&mut self) -> Option<Rule> {
        let mut selectors = Vec::new();

        loop {
            let Some(selector) = self.parse_selector() else {
                self.skip_past_block();
                return None;
            };
            selectors.push(selector);

            if self.eat(TokenKind::Comma).is_none() {
                break;
            }
        }

        let Some(open) = self.eat(TokenKind::LeftBrace) else {
            self.error(ParseErrorKind::ExpectedLeftBrace, self.span_here());
            self.skip_past_block();
            return None;
        };

        let declarations = self.parse_declarations(open.span);

        Some(Rule {
            selectors,
            declarations,
        })
    }

    fn parse_selector(&mut self) -> Option<Selector> {
        // Parse optional base
        let base = match self.peek().map(|token| token.kind) {
            Some(TokenKind::Dot) => {
                let dot = self.advance()?;

                if !self.at(TokenKind::Name) {
                    self.error(ParseErrorKind::ExpectedSelector, self.span_here());
                    return None;
                }

                let name = self.advance()?;
                self.check_touching(dot.span, name.span);

                Some((
                    SelectorKind::Class,
                    Span {
                        start: dot.span.start,
                        end: name.span.end,
                    },
                ))
            }
            Some(TokenKind::Hash) => {
                let hash = self.advance()?;
                Some((SelectorKind::Id, hash.span))
            }
            Some(TokenKind::Name) => {
                let name = self.advance()?;
                Some((SelectorKind::Type, name.span))
            }
            _ => None,
        };

        // Parse optional pseudo
        let pseudo = if self.at(TokenKind::Colon) {
            let colon = self.advance()?; // consume ':'

            if let Some((_, base_span)) = base {
                self.check_touching(base_span, colon.span);
            }

            if !self.at(TokenKind::Name) {
                self.error(ParseErrorKind::ExpectedSelector, self.span_here());
                return None;
            }

            let name = self.advance()?;
            self.check_touching(colon.span, name.span);
            Some(name.span)
        } else {
            None
        };

        // Neither base nor pseudo exists
        if base.is_none() && pseudo.is_none() {
            // Report ExpectedSelector here.
            self.error(ParseErrorKind::ExpectedSelector, self.span_here());
            return None;
        }

        // 4. Return the selector
        Some(Selector { base, pseudo })
    }

    fn parse_declarations(&mut self, open: Span) -> Vec<Declaration> {
        let mut declarations = Vec::new();

        loop {
            // Stop at the closing delimiter, leaving it for this function
            // to consume.
            if self.eat(TokenKind::RightBrace).is_some() {
                return declarations;
            }

            if self.peek().is_none() {
                // Report UnclosedBlock using `open`.
                self.error(ParseErrorKind::UnclosedBlock, open);
                return declarations;
            }

            if let Some(declaration) = self.parse_declaration() {
                declarations.push(declaration);
            }
        }
    }

    fn parse_inline_declarations(&mut self) -> Vec<Declaration> {
        let mut declarations = Vec::new();

        while let Some(token) = self.peek() {
            if token.kind == TokenKind::RightBrace {
                self.error(ParseErrorKind::ExpectedPropertyName, token.span);
                self.advance();
                continue;
            }

            if let Some(declaration) = self.parse_declaration() {
                declarations.push(declaration);
            }
        }

        declarations
    }

    fn parse_declaration(&mut self) -> Option<Declaration> {
        let Some(name) = self.peek() else {
            return None;
        };

        if name.kind != TokenKind::Name {
            // ExpectedPropertyName
            self.error(ParseErrorKind::ExpectedPropertyName, name.span);
            self.recover_declaration();
            return None;
        }

        let name = self.advance()?;

        if !self.at(TokenKind::Colon) {
            // ExpectedColon
            self.error(ParseErrorKind::ExpectedColon, self.span_here());
            self.recover_declaration();
            return None;
        }

        self.advance(); // consume ':'

        let mut value = Vec::new();

        loop {
            if self.at(TokenKind::Semicolon) {
                break;
            }

            if self.at(TokenKind::RightBrace) {
                break; // Leave `}` untouched.
            }

            let Some(_) = self.peek() else {
                break;
            };

            value.push(self.advance()?);
        }

        if value.is_empty() {
            // ExpectedValue
            self.error(ParseErrorKind::ExpectedValue, self.span_here());
            self.recover_declaration();
            return None;
        }

        self.eat(TokenKind::Semicolon); // consume ';'

        Some(Declaration {
            name: name.span,
            value,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn text(src: &str, span: Span) -> &str {
        &src[span.start..span.end]
    }

    fn texts<'a>(src: &'a str, tokens: &[Token]) -> Vec<&'a str> {
        tokens.iter().map(|t| text(src, t.span)).collect()
    }

    fn kinds(errors: &[ParseError]) -> Vec<ParseErrorKind> {
        errors.iter().map(|e| e.kind).collect()
    }

    fn selector<'a>(
        src: &'a str,
        selector: &Selector,
    ) -> (Option<(SelectorKind, &'a str)>, Option<&'a str>) {
        (
            selector.base.map(|(kind, span)| (kind, text(src, span))),
            selector.pseudo.map(|span| text(src, span)),
        )
    }

    fn declarations<'a>(src: &'a str, rule: &Rule) -> Vec<(&'a str, Vec<&'a str>)> {
        rule.declarations
            .iter()
            .map(|d| (text(src, d.name), texts(src, &d.value)))
            .collect()
    }

    fn only_rule(sheet: &Stylesheet) -> &Rule {
        let [Item::Rule(rule)] = sheet.items.as_slice() else {
            panic!("expected exactly one rule, got {sheet:?}");
        };
        rule
    }

    #[test]
    fn rule_with_selector_list() {
        let src = ".tab:hover, #root { width: 100%; background-color: var(--bg-deep); }";
        let (sheet, errors) = parse(src);
        assert!(errors.is_empty(), "{errors:?}");
        let rule = only_rule(&sheet);
        assert_eq!(
            selector(src, &rule.selectors[0]),
            (Some((SelectorKind::Class, ".tab")), Some("hover"))
        );
        assert_eq!(
            selector(src, &rule.selectors[1]),
            (Some((SelectorKind::Id, "#root")), None)
        );
        assert_eq!(
            declarations(src, rule),
            [
                ("width", vec!["100%"]),
                ("background-color", vec!["var", "(", "--bg-deep", ")"]),
            ]
        );
    }

    #[test]
    fn root_selector_has_no_base() {
        let src = ":root { --bg-deep: #101010; }";
        let (sheet, errors) = parse(src);
        assert!(errors.is_empty(), "{errors:?}");
        let rule = only_rule(&sheet);
        assert_eq!(selector(src, &rule.selectors[0]), (None, Some("root")));
        assert_eq!(declarations(src, rule), [("--bg-deep", vec!["#101010"])]);
    }

    #[test]
    fn type_selector() {
        let src = "Vec3Field:pressed { flex-direction: column; }";
        let (sheet, errors) = parse(src);
        assert!(errors.is_empty(), "{errors:?}");
        assert_eq!(
            selector(src, &only_rule(&sheet).selectors[0]),
            (Some((SelectorKind::Type, "Vec3Field")), Some("pressed"))
        );
    }

    #[test]
    fn last_semicolon_is_optional() {
        let src = ".a { gap: 2px }";
        let (sheet, errors) = parse(src);
        assert!(errors.is_empty(), "{errors:?}");
        assert_eq!(declarations(src, only_rule(&sheet)), [("gap", vec!["2px"])]);
    }

    #[test]
    fn at_rule() {
        let src = "@container (max-width: 220px) { Vec3Field { flex-direction: column; } }";
        let (sheet, errors) = parse(src);
        assert!(errors.is_empty(), "{errors:?}");
        let [Item::AtRule(at)] = sheet.items.as_slice() else {
            panic!("expected one at-rule, got {sheet:?}");
        };
        assert_eq!(text(src, at.name), "@container");
        assert_eq!(texts(src, &at.condition), ["max-width", ":", "220px"]);
        assert_eq!(at.rules.len(), 1);
    }

    #[test]
    fn missing_colon_skips_only_that_declaration() {
        let src = ".a { width 100%; height: 5px; }";
        let (sheet, errors) = parse(src);
        assert_eq!(kinds(&errors), [ParseErrorKind::ExpectedColon]);
        assert_eq!(
            declarations(src, only_rule(&sheet)),
            [("height", vec!["5px"])]
        );
    }

    #[test]
    fn missing_property_name() {
        let src = ".a { : red; gap: 2px; }";
        let (sheet, errors) = parse(src);
        assert_eq!(kinds(&errors), [ParseErrorKind::ExpectedPropertyName]);
        assert_eq!(declarations(src, only_rule(&sheet)), [("gap", vec!["2px"])]);
    }

    #[test]
    fn empty_value() {
        let (_, errors) = parse(".a { gap: ; }");
        assert_eq!(kinds(&errors), [ParseErrorKind::ExpectedValue]);
    }

    #[test]
    fn unclosed_block_points_at_its_brace() {
        let (_, errors) = parse(".a { gap: 2px;");
        assert_eq!(
            errors,
            [ParseError {
                kind: ParseErrorKind::UnclosedBlock,
                span: Span { start: 3, end: 4 },
            }]
        );
    }

    #[test]
    fn lex_errors_are_reported() {
        let (_, errors) = parse(".a { margin: - 4px; }");
        assert_eq!(
            kinds(&errors),
            [ParseErrorKind::Lex(LexError::UnexpectedByte)]
        );
    }

    #[test]
    fn bad_selector_skips_its_block() {
        let (sheet, errors) = parse("{ gap: 2px; } .b { gap: 1px; }");
        assert_eq!(kinds(&errors), [ParseErrorKind::ExpectedSelector]);
        assert_eq!(sheet.items.len(), 1);
    }

    #[test]
    fn descendant_selector_is_rejected() {
        let (sheet, errors) = parse(".a .b { gap: 1px; } .c { gap: 2px; }");
        assert_eq!(kinds(&errors), [ParseErrorKind::ExpectedLeftBrace]);
        assert_eq!(sheet.items.len(), 1);
    }

    #[test]
    fn selector_pieces_must_touch() {
        for src in [". tab { gap: 1px; }", ".tab :hover { gap: 1px; }"] {
            let (sheet, errors) = parse(src);
            assert_eq!(kinds(&errors), [ParseErrorKind::SpaceInSelector], "{src}");
            assert_eq!(sheet.items.len(), 1, "{src}");
        }
    }

    #[test]
    fn stray_tokens_report_errors_and_terminate() {
        for src in [
            "}", ";", "{", "(", ")", ",", ".", ":", "@", "#", "@media", "@media (", ".a", ".a {",
        ] {
            let (_, errors) = parse(src);
            assert!(!errors.is_empty(), "{src:?}");
        }
    }

    #[test]
    fn inline_declarations() {
        let src = "width: 100%; gap: 2px";
        let (declarations, errors) = parse_inline(src);
        assert!(errors.is_empty(), "{errors:?}");
        assert_eq!(
            declarations
                .iter()
                .map(|d| (text(src, d.name), texts(src, &d.value)))
                .collect::<Vec<_>>(),
            [("width", vec!["100%"]), ("gap", vec!["2px"])]
        );
    }

    #[test]
    fn inline_stray_brace_reports_and_terminates() {
        let (declarations, errors) = parse_inline("gap: 2px } width: 1px");
        assert_eq!(kinds(&errors), [ParseErrorKind::ExpectedPropertyName]);
        assert_eq!(declarations.len(), 2);
    }
}
