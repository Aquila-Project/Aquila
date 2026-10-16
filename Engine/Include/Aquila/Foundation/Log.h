#ifndef AQUILA_LOG_H
#define AQUILA_LOG_H

#include "Aquila/Foundation/Color.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include <ostream>

namespace Aquila::Foundation {

enum class LogLevel : Uint8 {
	Trace = 1 << 2,
	Debug = 1 << 3,
	Info = 1 << 4,
	Warning = 1 << 5,
	Error = 1 << 6,
	Critical = 1 << 7
};

class Logger {
  private:
	static LogLevel s_currentLevel;
	static bool s_showTimestamp;
	static bool s_showLocation;
	static bool s_useColors;

  public:
	static void set_log_level(LogLevel level);
	static LogLevel get_log_level();

	static void set_sink(std::ostream *buf);

	static void enable_timestamp(bool enable = true);
	static void enable_location(bool enable = true);
	static void enable_colors(bool enable = true);

  private:
	static std::ostream *s_sink;
	static std::string get_timestamp();
	static std::string get_level_string(LogLevel level);
	static std::string format_location(const std::source_location &location);
	static const char *get_level_color(LogLevel level);

	static bool is_enabled(LogLevel level);
	static void write(LogLevel level, std::string_view message, const std::source_location &location);

	template <typename... Args>
	static void log_impl_internal(LogLevel level, std::string_view format_str, const std::source_location &location,
								  Args &&...args) {
		if (!is_enabled(level)) {
			return;
		}

		if constexpr (sizeof...(args) > 0) {
			write(level, std::vformat(format_str, std::make_format_args(args...)), location);
		} else {
			write(level, format_str, location);
		}
	}

  public:
	template <typename... Args>
	static void log_trace(std::string_view format_str, Args &&...args,
						  const std::source_location &location = std::source_location::current()) {
		log_impl_internal(LogLevel::Trace, format_str, location, std::forward<Args>(args)...);
	}

	template <typename... Args>
	static void log_debug(std::string_view format_str, Args &&...args,
						  const std::source_location &location = std::source_location::current()) {
		log_impl_internal(LogLevel::Debug, format_str, location, std::forward<Args>(args)...);
	}

	template <typename... Args>
	static void log_info(std::string_view format_str, Args &&...args,
						 const std::source_location &location = std::source_location::current()) {
		log_impl_internal(LogLevel::Info, format_str, location, std::forward<Args>(args)...);
	}

	template <typename... Args>
	static void log_warning(std::string_view format_str, Args &&...args,
							const std::source_location &location = std::source_location::current()) {
		log_impl_internal(LogLevel::Warning, format_str, location, std::forward<Args>(args)...);
	}

	template <typename... Args>
	static void log_error(std::string_view format_str, Args &&...args,
						  const std::source_location &location = std::source_location::current()) {
		log_impl_internal(LogLevel::Error, format_str, location, std::forward<Args>(args)...);
	}

	template <typename... Args>
	static void log_critical(std::string_view format_str, Args &&...args,
							 const std::source_location &location = std::source_location::current()) {
		log_impl_internal(LogLevel::Critical, format_str, location, std::forward<Args>(args)...);
	}

	template <typename... Args> static void simple_log_trace(std::string_view format_str, Args &&...args) {
		log_impl_internal(LogLevel::Trace, format_str, std::source_location::current(), std::forward<Args>(args)...);
	}

	template <typename... Args> static void simple_log_debug(std::string_view format_str, Args &&...args) {
		log_impl_internal(LogLevel::Debug, format_str, std::source_location::current(), std::forward<Args>(args)...);
	}

	template <typename... Args> static void simple_log_info(std::string_view format_str, Args &&...args) {
		log_impl_internal(LogLevel::Info, format_str, std::source_location::current(), std::forward<Args>(args)...);
	}

	template <typename... Args> static void simple_log_warning(std::string_view format_str, Args &&...args) {
		log_impl_internal(LogLevel::Warning, format_str, std::source_location::current(), std::forward<Args>(args)...);
	}

	template <typename... Args> static void simple_log_error(std::string_view format_str, Args &&...args) {
		log_impl_internal(LogLevel::Error, format_str, std::source_location::current(), std::forward<Args>(args)...);
	}

	template <typename... Args> static void simple_log_critical(std::string_view format_str, Args &&...args) {
		log_impl_internal(LogLevel::Critical, format_str, std::source_location::current(), std::forward<Args>(args)...);
	}
};

template <typename... Args> void log(std::string_view format_str, Args &&...args) {
	Logger::simple_log_info(format_str, std::forward<Args>(args)...);
}

template <typename... Args> void log_error(std::string_view format_str, Args &&...args) {
	Logger::simple_log_error(format_str, std::forward<Args>(args)...);
}

template <typename... Args> void log_warning(std::string_view format_str, Args &&...args) {
	Logger::simple_log_warning(format_str, std::forward<Args>(args)...);
}

template <typename... Args> void log_trace(std::string_view format_str, Args &&...args) {
	Logger::simple_log_trace(format_str, std::forward<Args>(args)...);
}

template <typename... Args> void log_debug(std::string_view format_str, Args &&...args) {
	Logger::simple_log_debug(format_str, std::forward<Args>(args)...);
}

template <typename... Args> void log_critical(std::string_view format_str, Args &&...args) {
	Logger::simple_log_critical(format_str, std::forward<Args>(args)...);
}

template <typename... Args> void log_info(std::string_view format_str, Args &&...args) {
	Logger::simple_log_info(format_str, std::forward<Args>(args)...);
}

template <typename... Args>
[[noreturn]] void assert_failed(std::string_view condition, std::string_view format_str, Args &&...args,
								const std::source_location &location) {
	std::string message;
	if constexpr (sizeof...(args) > 0) {
		message = std::format(format_str, std::forward<Args>(args)...);
	} else {
		message = std::string{ format_str };
	}

	std::cerr << std::format("{}{}ASSERTION FAILED: {}{}\n", Color::BOLD, Color::BRIGHT_RED, condition, Color::RESET);
	std::cerr << std::format("{}Message: {}{}\n", Color::BRIGHT_YELLOW, message, Color::RESET);
	std::cerr << std::format("{}File: {}:{} in {}{}\n", Color::DIM, location.file_name(), location.line(),
							 location.function_name(), Color::RESET);

	std::terminate();
}

[[noreturn]] inline void assert_failed(const char *condition, const char *message, const char *file, int line) {
	std::cerr << std::format("{}{}ASSERTION FAILED: {}{}\n", Color::BOLD, Color::BRIGHT_RED, condition, Color::RESET);
	std::cerr << std::format("{}Message: {}{}\n", Color::BRIGHT_YELLOW, message, Color::RESET);
	std::cerr << std::format("{}File: {}:{}{}\n", Color::DIM, file, line, Color::RESET);
	std::terminate();
}

} // namespace Aquila::Foundation

#endif
