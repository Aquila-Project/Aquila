use std::fmt;

use crate::lexer::LexError::{EmptyHash, UnexpectedByte, UnterminatedComment};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Span {
    pub start: usize,
    pub end: usize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LexError {
    UnexpectedByte,
    UnterminatedComment,
    EmptyHash,
}

impl fmt::Display for LexError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(match self {
            UnexpectedByte => "unexpected character",
            UnterminatedComment => "this comment is never closed with `*/`",
            EmptyHash => "`#` must be followed by an id or a color",
        })
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TokenKind {
    Name,
    Hash,
    AtKeyword,
    Number { unit_start: Option<usize> },
    LeftBrace,
    RightBrace,
    LeftParen,
    RightParen,
    Colon,
    Semicolon,
    Comma,
    Dot,
    Comment,
    Error(LexError),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Token {
    pub kind: TokenKind,
    pub span: Span,
}

impl Token {
    pub fn text<'a>(&self, src: &'a str) -> &'a str {
        &src[self.span.start..self.span.end]
    }
}

pub fn line_col(src: &str, offset: usize) -> (usize, usize) {
    let before = &src[..offset];

    let line = before.bytes().filter(|&b| b == b'\n').count() + 1;

    let column = before.rsplit('\n').next().unwrap().chars().count() + 1;

    (line, column)
}

// A name can start with an ASCII letter, underscore, or hyphen.
// Examples: background-color, _private, --bg-deep, -custom
fn is_name_start(byte: u8) -> bool {
    byte.is_ascii_alphabetic() || byte == b'_' || byte == b'-'
}

// A name can continue with ASCII letters, digits, underscores, or hyphens.
// Examples: background-color, on-click, flex-direction, color2, --bg-deep
// Not valid continuation characters: space, @, :, .
fn is_name_continue(byte: u8) -> bool {
    byte.is_ascii_alphanumeric() || byte == b'_' || byte == b'-'
}

pub struct Lexer<'src> {
    src: &'src [u8],
    pos: usize,
}

impl<'src> Lexer<'src> {
    pub fn new(src: &'src str) -> Self {
        Self {
            src: src.as_bytes(),
            pos: 0,
        }
    }

    fn peek(&self) -> Option<u8> {
        // look at the current byte
        self.src.get(self.pos).copied()
    }

    fn peek_next(&self) -> Option<u8> {
        // look at the next byte
        self.src.get(self.pos + 1).copied()
    }

    pub fn advance(&mut self) -> Option<u8> {
        let byte = self.peek()?; // get byte
        self.pos += 1; // move over
        Some(byte) // return whatever byte we get next
    }

    fn eat_while(&mut self, pred: impl Fn(u8) -> bool) {
        while let Some(current_byte) = self.peek() {
            if pred(current_byte) {
                self.advance();
            } else {
                break;
            }
        }
    }

    fn token(&self, kind: TokenKind, start: usize) -> Token {
        Token {
            kind,
            span: Span {
                start,
                end: self.pos,
            },
        }
    }

    fn skip_whitespace(&mut self) {
        while let Some(current_byte) = self.peek() {
            match current_byte {
                b' ' | b'\t' | b'\r' | b'\n' => self.advance(), // skip space, tab, return
                _ => break,                                     // anything else, break the loop
            };
        }
    }

    fn unexpected(&mut self, start: usize) -> Token {
        self.eat_while(|b| b & 0b1100_0000 == 0b1000_0000);
        self.token(TokenKind::Error(UnexpectedByte), start)
    }

    fn slash(&mut self, start: usize) -> Token {
        let byte = self.peek();

        match byte {
            /* // case */
            Some(b'/') => {
                self.advance();
                self.eat_while(|b| b != b'\n');
                self.token(TokenKind::Comment, start)
            }

            // /* */ case
            Some(b'*') => {
                self.advance(); // consume *, we have now /* 

                // need to search further for closing */
                loop {
                    match self.advance() {
                        None => {
                            return self.token(TokenKind::Error(UnterminatedComment), start);
                        }

                        // if we found * while advancing, we should accept ONLY */ as a closing token for the comment
                        Some(b'*') if self.peek() == Some(b'/') => {
                            self.advance(); // consume /
                            break;
                        }
                        _ => {}
                    }
                }

                self.token(TokenKind::Comment, start)
            }

            _ => self.unexpected(start),
        }
    }

    fn minus(&mut self, start: usize) -> Token {
        match self.peek() {
            // Examples: -4px, -12,
            Some(b'0'..=b'9') => self.number(start),
            Some(b'.') if self.peek_next().is_some_and(|b| b.is_ascii_digit()) => {
                self.number(start)
            }

            // Examples: --bg-deep
            Some(b) if is_name_start(b) => self.name(start),

            // Examples: -, - followed by a space, - followed by punctuation
            _ => self.unexpected(start),
        }
    }

    fn name(&mut self, start: usize) -> Token {
        self.eat_while(is_name_continue);
        self.token(TokenKind::Name, start)
    }

    fn hash(&mut self, start: usize) -> Token {
        let byte = self.peek();

        if byte.is_some_and(is_name_continue) {
            self.eat_while(is_name_continue);
            self.token(TokenKind::Hash, start)
        } else {
            self.token(TokenKind::Error(EmptyHash), start)
        }
    }

    fn at_keyword(&mut self, start: usize) -> Token {
        let byte = self.peek();

        if byte.is_some_and(is_name_start) {
            self.eat_while(is_name_continue);
            self.token(TokenKind::AtKeyword, start)
        } else {
            self.token(TokenKind::Error(UnexpectedByte), start)
        }
    }

    fn number(&mut self, start: usize) -> Token {
        // Consume the remaining integer digits.
        // Examples: 12, -42, 100
        self.eat_while(|b| b.is_ascii_digit());

        // Consume an optional decimal part.
        // Examples: 0.5, 3.14
        if self.peek() == Some(b'.') && self.peek_next().is_some_and(|b| b.is_ascii_digit()) {
            self.advance(); // Consume '.'
            self.eat_while(|b| b.is_ascii_digit());
        }

        let unit_start = if self.peek() == Some(b'%') {
            let unit = self.pos;
            self.advance();
            Some(unit)
        } else if self.peek().is_some_and(|b| b.is_ascii_alphabetic()) {
            let unit = self.pos;
            self.eat_while(is_name_continue);
            Some(unit)
        } else {
            None
        };

        self.token(TokenKind::Number { unit_start }, start)
    }
}

impl Iterator for Lexer<'_> {
    type Item = Token;

    fn next(&mut self) -> Option<Token> {
        self.skip_whitespace();
        let start = self.pos;
        let byte = self.advance()?;
        let token = match byte {
            b'{' => self.token(TokenKind::LeftBrace, start),
            b'}' => self.token(TokenKind::RightBrace, start),
            b'(' => self.token(TokenKind::LeftParen, start),
            b')' => self.token(TokenKind::RightParen, start),
            b':' => self.token(TokenKind::Colon, start),
            b';' => self.token(TokenKind::Semicolon, start),
            b',' => self.token(TokenKind::Comma, start),
            b'.' => {
                if self.peek().is_some_and(|b| b.is_ascii_digit()) {
                    self.number(start)
                } else {
                    self.token(TokenKind::Dot, start)
                }
            }
            b'%' => self.unexpected(start),
            b'/' => self.slash(start),
            b'-' => self.minus(start),
            b'#' => self.hash(start),
            b'@' => self.at_keyword(start),
            b'0'..=b'9' => self.number(start),
            b if is_name_start(b) => self.name(start),
            _ => self.unexpected(start),
        };
        Some(token)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn texts(src: &str) -> Vec<&str> {
        Lexer::new(src).map(|t| t.text(src)).collect()
    }

    fn kinds(src: &str) -> Vec<TokenKind> {
        Lexer::new(src).map(|t| t.kind).collect()
    }

    #[test]
    fn declaration_with_percentage() {
        assert_eq!(texts("width: 100%;"), ["width", ":", "100%", ";"]);
    }

    #[test]
    fn variable_definition_with_color() {
        assert_eq!(
            texts("--bg-deep: #101010;"),
            ["--bg-deep", ":", "#101010", ";"]
        );
    }

    #[test]
    fn colors_and_ids_are_both_hash() {
        assert_eq!(kinds("#root #add"), [TokenKind::Hash, TokenKind::Hash]);
    }

    #[test]
    fn empty_hash_is_an_error() {
        assert_eq!(kinds("# {")[0], TokenKind::Error(LexError::EmptyHash));
    }

    #[test]
    fn class_selector() {
        assert_eq!(texts(".dock-space {"), [".", "dock-space", "{"]);
    }

    #[test]
    fn media_query() {
        assert_eq!(
            texts("@media (max-width: 500px) {"),
            ["@media", "(", "max-width", ":", "500px", ")", "{"]
        );
    }

    #[test]
    fn number_remembers_its_unit() {
        assert_eq!(
            kinds("100% 28"),
            [
                TokenKind::Number {
                    unit_start: Some(3)
                },
                TokenKind::Number { unit_start: None },
            ]
        );
    }

    #[test]
    fn glued_minus_differs_from_spaced_minus() {
        assert_ne!(texts("margin: -4px;"), texts("margin: - 4px;"));
    }

    #[test]
    fn comments() {
        assert_eq!(texts("a /* x */ b // y"), ["a", "/* x */", "b", "// y"]);
        assert_eq!(
            kinds("a /* x */ b // y"),
            [
                TokenKind::Name,
                TokenKind::Comment,
                TokenKind::Name,
                TokenKind::Comment
            ]
        );
    }

    #[test]
    fn unterminated_comment_points_at_its_start() {
        let src = "a /* b";
        let last = Lexer::new(src).last().unwrap();
        assert_eq!(last.kind, TokenKind::Error(LexError::UnterminatedComment));
        assert_eq!(last.span.start, 2);
    }

    #[test]
    fn unexpected_bytes_do_not_stop_the_lexer() {
        assert_eq!(texts("a $ é b"), ["a", "$", "é", "b"]);
        assert_eq!(
            kinds("a $ é b"),
            [
                TokenKind::Name,
                TokenKind::Error(LexError::UnexpectedByte),
                TokenKind::Error(LexError::UnexpectedByte),
                TokenKind::Name,
            ]
        );
    }

    #[test]
    fn line_and_column() {
        assert_eq!(line_col("ab", 0), (1, 1));
        assert_eq!(line_col("ab\ncd", 4), (2, 2));
    }
}
