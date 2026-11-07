#include "UI/Panels/ConsolePanel.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/Log.h"
#include "Aquila/UI/Style/StyleProperties.h"

namespace Editor {

using namespace Aquila;
using namespace Aquila::Foundation;
using namespace Aquila::GFX;
using namespace Aquila::UI;

int LogCaptureBuf::overflow(int c) {
	if (c == traits_type::eof()) {
		return traits_type::eof();
	}
	if (static_cast<char>(c) == '\n') {
		if (m_callback && !m_line.empty()) {
			m_callback(m_line);
		}
		m_line.clear();
	} else {
		m_line += static_cast<char>(c);
	}
	return c;
}

std::streamsize LogCaptureBuf::xsputn(const char *s, std::streamsize n) {
	for (std::streamsize i = 0; i < n; ++i) {
		overflow(static_cast<unsigned char>(s[i]));
	}
	return n;
}

ConsolePanel::ConsolePanel() {
	m_capture_buf.set_callback([this](std::string line) {
		{
			const std::scoped_lock lock(m_pending_mutex);
			m_pending.push_back(std::move(line));
		}
		FrameScheduler::get()->request_frame();
	});
	Logger::enable_colors(false);
	Logger::set_sink(&m_capture_stream);
}

ConsolePanel::~ConsolePanel() {
	Logger::set_sink(nullptr);
}

void ConsolePanel::build(UI::Core::View *panel, UI::Core::View * /*overlayRoot*/) {
	m_scroll_view = panel->find_by_id<UI::Core::ScrollView>("console-scroll");
	m_view = panel->find_by_id<UI::Core::SelectableTextView>("console-log");

	auto wire_filter = [&](std::string_view name, FilterGroup group, UI::Core::View *&button_out,
						   UI::Core::Label *&count_out) {
		auto *button = panel->find_by_id<UI::Core::Clickable>("console-filter-" + std::string(name));
		count_out = panel->find_by_id<UI::Core::Label>("console-count-" + std::string(name));
		button_out = button;
		if (button != nullptr) {
			button->on_click.connect([this, group]() { toggle_filter(group); });
		}
	};
	wire_filter("info", FilterGroup::Info, m_info_filter_btn, m_info_count_label);
	wire_filter("warning", FilterGroup::Warning, m_warning_filter_btn, m_warning_count_label);
	wire_filter("error", FilterGroup::Error, m_error_filter_btn, m_error_count_label);
}

void ConsolePanel::flush_pending() {
	if ((m_scroll_view == nullptr) || (m_view == nullptr)) {
		return;
	}
	std::vector<std::string> lines;
	{
		const std::scoped_lock lock(m_pending_mutex);
		lines.swap(m_pending);
	}
	if (lines.empty()) {
		return;
	}
	const bool follow_output = m_scroll_view->is_at_bottom();
	for (auto &line : lines) {
		const bool continuation = line.find("[AQUILA ") == std::string::npos && !m_entries.empty();
		const LogLevel level = continuation ? m_entries.back().level : parse_level(line);
		const std::string_view tag = level_tag(level);
		const Usize tag_pos = tag.empty() || continuation ? std::string::npos : line.find(tag);
		std::vector<UI::Core::TextSpan> spans;
		if (tag_pos != std::string::npos) {
			spans.push_back({ .begin = static_cast<Int32>(tag_pos),
							  .length = static_cast<Int32>(tag.size()),
							  .style_class = level_tag_class(level) });
		}
		LogEntry entry{ level, continuation ? "    " + line : std::move(line), std::move(spans) };
		entry.continuation = continuation;
		append_entry(std::move(entry));
	}
	update_filter_buttons();
	if (follow_output) {
		m_scroll_view->scroll_to_bottom();
	}
}

void ConsolePanel::append_entry(LogEntry entry) {
	if ((int)m_entries.size() >= K_MAX_MESSAGES) {
		const LogEntry &oldest = m_entries.front();
		if (!oldest.continuation) {
			--count_for(oldest.level);
		}
		if (is_shown(oldest.level)) {
			m_view->remove_front(1);
		}
		m_entries.erase(m_entries.begin());
	}

	if (!entry.continuation) {
		++count_for(entry.level);
	}

	if (is_shown(entry.level)) {
		show_entry(entry);
	}
	m_entries.push_back(std::move(entry));
}

void ConsolePanel::clear_all() {
	if (m_view != nullptr) {
		m_view->clear();
	}
	m_entries.clear();
	m_info_count = 0;
	m_warning_count = 0;
	m_error_count = 0;
	update_filter_buttons();
}

void ConsolePanel::toggle_filter(FilterGroup group) {
	switch (group) {
	case FilterGroup::Info:
		m_show_info = !m_show_info;
		if (m_info_filter_btn != nullptr) {
			m_info_filter_btn->set_class("dimmed", !m_show_info);
		}
		break;
	case FilterGroup::Warning:
		m_show_warning = !m_show_warning;
		if (m_warning_filter_btn != nullptr) {
			m_warning_filter_btn->set_class("dimmed", !m_show_warning);
		}
		break;
	case FilterGroup::Error:
		m_show_error = !m_show_error;
		if (m_error_filter_btn != nullptr) {
			m_error_filter_btn->set_class("dimmed", !m_show_error);
		}
		break;
	}
	rebuild_view();
}

bool ConsolePanel::is_shown(LogLevel level) const {
	switch (level_to_group(level)) {
	case FilterGroup::Info:
		return m_show_info;
	case FilterGroup::Warning:
		return m_show_warning;
	case FilterGroup::Error:
		return m_show_error;
	}
	return true;
}

void ConsolePanel::show_entry(const LogEntry &entry) {
	m_view->add_line(entry.message, {}, entry.spans);
}

void ConsolePanel::rebuild_view() {
	if (m_view == nullptr) {
		return;
	}
	m_view->clear();
	for (const LogEntry &entry : m_entries) {
		if (is_shown(entry.level)) {
			show_entry(entry);
		}
	}
}

void ConsolePanel::update_filter_buttons() {
	if (m_info_count_label != nullptr) {
		m_info_count_label->set_text(std::to_string(m_info_count));
	}
	if (m_warning_count_label != nullptr) {
		m_warning_count_label->set_text(std::to_string(m_warning_count));
	}
	if (m_error_count_label != nullptr) {
		m_error_count_label->set_text(std::to_string(m_error_count));
	}
}

LogLevel ConsolePanel::parse_level(const std::string &line) {
	if (line.find("[AQUILA CRITICAL]") != std::string::npos) {
		return LogLevel::Critical;
	}
	if (line.find("[AQUILA ERROR]") != std::string::npos) {
		return LogLevel::Error;
	}
	if (line.find("[AQUILA WARNING]") != std::string::npos) {
		return LogLevel::Warning;
	}
	if (line.find("[AQUILA DEBUG]") != std::string::npos) {
		return LogLevel::Debug;
	}
	if (line.find("[AQUILA TRACE]") != std::string::npos) {
		return LogLevel::Trace;
	}
	return LogLevel::Info;
}

int &ConsolePanel::count_for(LogLevel level) {
	switch (level_to_group(level)) {
	case FilterGroup::Warning:
		return m_warning_count;
	case FilterGroup::Error:
		return m_error_count;
	case FilterGroup::Info:
		break;
	}
	return m_info_count;
}

ConsolePanel::FilterGroup ConsolePanel::level_to_group(LogLevel level) {
	switch (level) {
	case LogLevel::Warning:
		return FilterGroup::Warning;
	case LogLevel::Error:
		return FilterGroup::Error;
	case LogLevel::Critical:
		return FilterGroup::Error;
	default:
		return FilterGroup::Info;
	}
}

std::string ConsolePanel::level_tag_class(LogLevel level) {
	switch (level) {
	case LogLevel::Warning:
		return "console-tag-warning";
	case LogLevel::Error:
		return "console-tag-error";
	case LogLevel::Critical:
		return "console-tag-critical";
	case LogLevel::Debug:
		return "console-tag-debug";
	case LogLevel::Trace:
		return "console-tag-trace";
	default:
		return "console-tag-info";
	}
}

std::string_view ConsolePanel::level_tag(LogLevel level) {
	switch (level) {
	case LogLevel::Warning:
		return "[AQUILA WARNING]";
	case LogLevel::Error:
		return "[AQUILA ERROR]";
	case LogLevel::Critical:
		return "[AQUILA CRITICAL]";
	case LogLevel::Debug:
		return "[AQUILA DEBUG]";
	case LogLevel::Trace:
		return "[AQUILA TRACE]";
	default:
		return "[AQUILA INFO]";
	}
}

} // namespace Editor
