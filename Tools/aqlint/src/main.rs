mod macros;

use aqlint::{layout, lexer, lexer::Span, parser};
use colored::Colorize;
use macros::{lint_error, lint_note, lint_warning, progress, usage, verbose};
use std::{
    fmt, fs, io,
    path::{Path, PathBuf},
    process::ExitCode,
};

use crate::macros::{is_enable_tokens, is_help};

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum FileKind {
    Layout,
    Style,
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct SourceFile {
    pub path: PathBuf,
    pub kind: FileKind,
}

#[derive(Debug)]
pub enum InputError {
    Io { path: PathBuf, source: io::Error },
    UnknownExtension { path: PathBuf },
    UnknownOption(String),
}

impl InputError {
    fn io(path: impl Into<PathBuf>, source: io::Error) -> Self {
        InputError::Io {
            path: path.into(),
            source,
        }
    }
}

impl fmt::Display for InputError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            InputError::Io { path, source } => write!(f, "{}: {source}", path.display()),
            InputError::UnknownExtension { path } => write!(
                f,
                "{} has an unknown extension. Use aqlint on .aqlayout and .aqstyle files",
                path.display()
            ),
            InputError::UnknownOption(flag) => write!(f, "unknown option: {flag}"),
        }
    }
}

impl std::error::Error for InputError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            InputError::Io { source, .. } => Some(source),
            InputError::UnknownExtension { .. } | InputError::UnknownOption(_) => None,
        }
    }
}

pub fn file_kind(path: &Path) -> Option<FileKind> {
    match path.extension()?.to_str()? {
        "aqlayout" => Some(FileKind::Layout),
        "aqstyle" => Some(FileKind::Style),
        _ => None,
    }
}

fn collect_dir(dir: &Path, files: &mut Vec<SourceFile>, errors: &mut Vec<InputError>) {
    let entries = match fs::read_dir(dir) {
        Ok(entries) => entries,
        Err(e) => return errors.push(InputError::io(dir, e)),
    };

    for entry in entries {
        let (path, is_dir) = match entry {
            Ok(entry) => {
                match entry.file_name().to_str() {
                    Some(n) if n.starts_with('.') => continue, // if it starts with . -> skip
                    Some("build" | "target") => continue,      // if its build or target -> skip
                    _ => {}
                }
                let is_dir = match entry.file_type() {
                    Ok(ft) => ft.is_dir(),
                    Err(e) => {
                        errors.push(InputError::io(entry.path(), e));
                        continue;
                    }
                };
                (entry.path(), is_dir)
            }
            Err(e) => {
                errors.push(InputError::io(dir, e));
                continue;
            }
        };

        if is_dir {
            collect_dir(&path, files, errors);
        } else if let Some(kind) = file_kind(&path) {
            files.push(SourceFile { path, kind });
        }
    }
}

fn collect_arg(arg: &str, files: &mut Vec<SourceFile>, errors: &mut Vec<InputError>) {
    let path = Path::new(arg);
    match fs::metadata(path) {
        Ok(meta) if meta.is_dir() => collect_dir(path, files, errors),
        Ok(_) => match file_kind(path) {
            Some(kind) => files.push(SourceFile {
                path: path.to_path_buf(),
                kind,
            }),
            None => errors.push(InputError::UnknownExtension {
                path: path.to_path_buf(),
            }),
        },
        Err(e) => errors.push(InputError::io(path, e)),
    }
}

fn print_tokens(file: &SourceFile, src: &str) {
    if file.kind != FileKind::Style {
        eprintln!(
            "{}",
            lint_warning!(
                "{}: --tokens only lexes .aqstyle files for now",
                file.path.display()
            )
        );
        return;
    }

    for token in lexer::Lexer::new(src) {
        let (line, col) = lexer::line_col(src, token.span.start);
        let kind = format!("{:?}", token.kind);
        println!(
            "{}:{:<9} {kind:<32} {:?}",
            file.path.display(),
            format!("{line}:{col}"),
            token.text(src)
        );
    }
}

struct Diagnostic {
    span: Span,
    message: String,
    note: Option<(String, Span)>,
}

fn diagnostics(kind: FileKind, src: &str) -> Vec<Diagnostic> {
    let mut diagnostics: Vec<Diagnostic> = match kind {
        FileKind::Style => parser::parse(src)
            .1
            .into_iter()
            .map(|e| Diagnostic {
                span: e.span,
                message: e.kind.to_string(),
                note: None,
            })
            .collect(),
        FileKind::Layout => layout::parse(src)
            .1
            .into_iter()
            .map(|e| Diagnostic {
                span: e.span,
                message: e.kind.to_string(),
                note: match e.kind {
                    layout::LayoutErrorKind::MismatchedClosingTag { open } => Some((
                        format!("`<{}>` is opened here", &src[open.start..open.end]),
                        open,
                    )),
                    _ => None,
                },
            })
            .collect(),
    };
    diagnostics.sort_by_key(|d| d.span.start);
    diagnostics
}

fn snippet(src: &str, span: Span) -> String {
    let line_start = src[..span.start].rfind('\n').map_or(0, |i| i + 1);
    let line_end = src[span.start..]
        .find('\n')
        .map_or(src.len(), |i| span.start + i);
    let line = src[line_start..line_end].trim_end_matches('\r');
    let indent: String = src[line_start..span.start]
        .chars()
        .map(|c| if c == '\t' { '\t' } else { ' ' })
        .collect();
    let width = src[span.start..span.end.clamp(span.start, line_end)]
        .chars()
        .count()
        .max(1);
    let (number, _) = lexer::line_col(src, span.start);
    let gutter = " ".repeat(number.to_string().len());
    let bar = "|".blue().bold();
    format!(
        "{} {bar} {line}\n{gutter} {bar} {indent}{}",
        number.to_string().blue().bold(),
        "^".repeat(width).red().bold()
    )
}

fn report(path: &Path, src: &str, diagnostic: &Diagnostic) {
    let (line, col) = lexer::line_col(src, diagnostic.span.start);
    eprintln!(
        "{}",
        lint_error!(at path.display(), line, col; "{}", diagnostic.message)
    );
    eprintln!("{}", snippet(src, diagnostic.span));
    if let Some((message, span)) = &diagnostic.note {
        let (line, col) = lexer::line_col(src, span.start);
        eprintln!(
            "{}",
            lint_note!("{message}: {}:{line}:{col}", path.display())
        );
    }
    eprintln!();
}

fn check_file(file: &SourceFile) -> Result<usize, InputError> {
    let src = fs::read_to_string(&file.path).map_err(|e| InputError::io(&file.path, e))?;
    if is_enable_tokens() {
        print_tokens(file, &src);
    }

    let diagnostics = diagnostics(file.kind, &src);
    for diagnostic in &diagnostics {
        report(&file.path, &src, diagnostic);
    }
    Ok(diagnostics.len())
}

fn plural(count: usize, noun: &str) -> String {
    if count == 1 {
        format!("1 {noun}")
    } else {
        format!("{count} {noun}s")
    }
}

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();

    let mut files = Vec::new();
    let mut errors = Vec::new();

    for arg in &args {
        match arg.as_str() {
            "-h" | "--help" => macros::set_help(true),
            "-v" | "--verbose" => macros::set_verbose(true),
            "-t" | "--tokens" => macros::set_enable_tokens(true),
            flag if flag.starts_with('-') => {
                errors.push(InputError::UnknownOption(flag.to_string()))
            }
            file => collect_arg(file, &mut files, &mut errors),
        }
    }

    files.sort();
    files.dedup();

    if is_help() {
        usage!();
        return ExitCode::SUCCESS;
    }

    if files.is_empty() && errors.is_empty() {
        usage!("no files to lint");
        return ExitCode::from(2);
    }

    if !errors.is_empty() {
        for e in &errors {
            eprintln!("{}", lint_error!("{e}"));
        }
        return ExitCode::from(2);
    }

    let mut error_count = 0;
    let mut failed_files = 0;
    let mut unreadable = false;
    for (i, file) in files.iter().enumerate() {
        progress!(i, files.len(); "Now checking: {}", file.path.display());
        match check_file(file) {
            Ok(0) => {}
            Ok(count) => {
                error_count += count;
                failed_files += 1;
            }
            Err(e) => {
                eprintln!("{}", lint_error!("{e}"));
                unreadable = true;
            }
        }
    }

    if error_count > 0 {
        eprintln!(
            "{}",
            lint_error!(
                "{} in {} of {}",
                plural(error_count, "error"),
                failed_files,
                plural(files.len(), "file")
            )
        );
    } else {
        verbose!("{} checked, no errors", plural(files.len(), "file"));
    }

    if unreadable {
        ExitCode::from(2)
    } else if error_count > 0 {
        ExitCode::from(1)
    } else {
        ExitCode::SUCCESS
    }
}
