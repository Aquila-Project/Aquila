#include "Aquila/Foundation/Log.h"

namespace Aquila::Foundation {

namespace {

std::mutex &write_mutex() {
	static std::mutex mutex;
	return mutex;
}

}

#ifdef AQUILA_DEBUG
LogLevel Logger::s_currentLevel = LogLevel::Debug;
#else
LogLevel Logger::s_currentLevel = LogLevel::Info;
#endif
bool Logger::s_showTimestamp = true;
bool Logger::s_showLocation = false;
bool Logger::s_useColors = true;
std::ostream *Logger::s_sink = nullptr;

void Logger::set_log_level(LogLevel level) {
	s_currentLevel = level;
}

LogLevel Logger::get_log_level() {
	return s_currentLevel;
}

void Logger::set_sink(std::ostream *buf) {
	const std::scoped_lock lock(write_mutex());
	s_sink = buf;
}

void Logger::enable_timestamp(bool enable) {
	s_showTimestamp = enable;
}

void Logger::enable_location(bool enable) {
	s_showLocation = enable;
}

void Logger::enable_colors(bool enable) {
	s_useColors = enable;
}

bool Logger::is_enabled(LogLevel level) {
	return level >= s_currentLevel;
}

const char *Logger::get_level_color(LogLevel level) {
	if (!s_useColors) {
		return "";
	}

	switch (level) {
	case LogLevel::Trace:
		return Color::BRIGHT_BLACK;
	case LogLevel::Debug:
		return Color::CYAN;
	case LogLevel::Info:
		return Color::BRIGHT_GREEN;
	case LogLevel::Warning:
		return Color::BRIGHT_YELLOW;
	case LogLevel::Error:
		return Color::BRIGHT_RED;
	case LogLevel::Critical: {
		static const std::string critical_color = std::format("{}{}", Color::BOLD, Color::BRIGHT_RED);
		return critical_color.c_str();
	}
	default:
		return Color::RESET;
	}
}

void Logger::write(LogLevel level, std::string_view message, const std::source_location &location) {
	const char *color = get_level_color(level);
	const char *reset = s_useColors ? Color::RESET : "";
	const char *dim_color = s_useColors ? Color::DIM : "";

	std::string prefix = std::format("{}[AQUILA {}]{}", color, get_level_string(level), reset);

	if (s_showTimestamp) {
		prefix = std::format("{}{}{} {}", dim_color, get_timestamp(), reset, prefix);
	}

	if (s_showLocation && level >= LogLevel::Warning) {
		prefix = std::format("{} {}{}{}", prefix, dim_color, format_location(location), reset);
	}

	const std::string line = std::format("{} {}\n", prefix, message);
	const std::scoped_lock lock(write_mutex());
	auto &stream = s_sink ? *s_sink : (level >= LogLevel::Error) ? std::cerr : std::cout;
	stream << line;
	stream.flush();
}

std::string Logger::get_timestamp() {
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	std::stringstream ss;

#ifdef AQUILA_PLATFORM_WINDOWS
	tm buf{};
	localtime_s(&buf, &time_t);
	ss << std::put_time(&buf, "%H:%M:%S");
#else
	tm buf{};
	localtime_r(&time_t, &buf);
	ss << std::put_time(&buf, "%H:%M:%S");
#endif

	ss << std::format(".{:03d}", ms.count());
	return ss.str();
}

std::string Logger::get_level_string(LogLevel level) {
	switch (level) {
	case LogLevel::Trace:
		return "TRACE";
	case LogLevel::Debug:
		return "DEBUG";
	case LogLevel::Info:
		return "INFO";
	case LogLevel::Warning:
		return "WARNING";
	case LogLevel::Error:
		return "ERROR";
	case LogLevel::Critical:
		return "CRITICAL";
	default:
		return "UNKNOWN";
	}
}

std::string Logger::format_location(const std::source_location &location) {
	std::string_view file_path = location.file_name();
	auto last_slash = file_path.find_last_of("/\\");
	std::string_view filename = (last_slash != std::string_view::npos) ? file_path.substr(last_slash + 1) : file_path;

	return std::format("({}:{})", filename, location.line());
}

} // namespace Aquila::Foundation
