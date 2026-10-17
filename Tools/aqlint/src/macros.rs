use std::sync::atomic::{AtomicBool, Ordering};

// Global verbose flag, set once from main.
static VERBOSE: AtomicBool = AtomicBool::new(false);
static HELP: AtomicBool = AtomicBool::new(false);
static ENABLE_TOKENS: AtomicBool = AtomicBool::new(false);

pub fn set_verbose(on: bool) {
    VERBOSE.store(on, Ordering::Relaxed);
}

pub fn set_help(on: bool) {
    HELP.store(on, Ordering::Relaxed);
}

pub fn set_enable_tokens(on: bool) {
    ENABLE_TOKENS.store(on, Ordering::Relaxed);
}

pub fn is_help() -> bool {
    HELP.load(Ordering::Relaxed)
}

pub fn is_enable_tokens() -> bool {
    ENABLE_TOKENS.load(Ordering::Relaxed)
}

pub fn is_verbose() -> bool {
    VERBOSE.load(Ordering::Relaxed)
}

// Internal helper: builds "label: message", or "file:line:col: label: message".
macro_rules! diag {
    ($label:expr, at $file:expr, $line:expr, $col:expr; $($arg:tt)*) => {
        format!("{}:{}:{}: {}: {}", $file, $line, $col, $label, format!($($arg)*))
    };
    ($label:expr; $($arg:tt)*) => {
        format!("{}: {}", $label, format!($($arg)*))
    };
}

/// Builds an error message String.
///   lint_error!("{path} does not exist")
///   lint_error!(at path, 12, 4; "unknown property `{name}`")
macro_rules! lint_error {
    (at $($rest:tt)*) => {{
        use ::colored::Colorize;
        $crate::macros::diag!("aqlint error".red().bold(), at $($rest)*)
    }};
    ($($arg:tt)*) => {{
        use ::colored::Colorize;
        $crate::macros::diag!("aqlint error".red().bold(); $($arg)*)
    }};
}

/// Builds a warning message String. Same syntax as lint_error!.
macro_rules! lint_warning {
    (at $($rest:tt)*) => {{
        use ::colored::Colorize;
        $crate::macros::diag!("aqlint warning".yellow().bold(), at $($rest)*)
    }};
    ($($arg:tt)*) => {{
        use ::colored::Colorize;
        $crate::macros::diag!("aqlint warning".yellow().bold(); $($arg)*)
    }};
}

/// Builds a note String, for extra context under an error or warning.
macro_rules! lint_note {
    ($($arg:tt)*) => {{
        use ::colored::Colorize;
        $crate::macros::diag!("  note".cyan(); $($arg)*)
    }};
}

/// Returns Err(lint_error!(...)) from the current function.
///   bail!("{path} is empty");
///   bail!(at path, line, col; "unexpected `{tok}`");
#[expect(unused_macros)]
macro_rules! bail {
    ($($arg:tt)*) => {
        return Err($crate::macros::lint_error!($($arg)*))
    };
}

/// Prints (dimmed) only when verbose mode is on.
macro_rules! verbose {
    ($($arg:tt)*) => {
        if $crate::macros::is_verbose() {
            use ::colored::Colorize;
            println!("{}", format!($($arg)*).dimmed());
        }
    };
}

macro_rules! progress {
    ($i:expr, $total:expr; $($arg:tt)*) => {
        if $crate::macros::is_verbose() {
            use ::colored::Colorize;
            let total = $total;
            let width = total.to_string().len();
            let counter = format!("[{:>width$}/{}]", $i + 1, total);
            println!("{} {}", counter.cyan().bold(), format!($($arg)*).dimmed());
        }
    };
}

/// usage!()                     -> prints help
/// usage!("unknown flag {f}")   -> prints an error, then help
macro_rules! usage {
    () => {{
        use ::colored::Colorize;
        println!(
            "{} {} [options] <files...>

{}
  -h, --help       Show this help message
  -v, --verbose    Print each file as it's checked
  -t, --tokens     Print the tokens of each .aqstyle file

{}
  aqlint main.aqlayout
  aqlint -v src/foo.aqlayout src/bar.aqstyle
  aqlint Resources/

{}
  0  no errors
  1  lint errors
  2  bad arguments or unreadable files",
            "Usage:".bold(),
            "aqlint".green().bold(),
            "Options:".bold(),
            "Examples:".bold(),
            "Exit status:".bold(),
        )
    }};
    ($($arg:tt)+) => {{
        eprintln!("{}\n", $crate::macros::lint_error!($($arg)+));
        $crate::macros::usage!();
    }};
}

pub(crate) use diag;
pub(crate) use lint_error;
pub(crate) use lint_note;
pub(crate) use lint_warning;
pub(crate) use progress;
pub(crate) use usage;
pub(crate) use verbose;
