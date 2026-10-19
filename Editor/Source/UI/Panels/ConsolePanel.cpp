#include "UI/Panels/ConsolePanel.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/Log.h"
#include "Aquila/UI/Style/StyleProperties.h"
#include "Aquila/GFX/GfxTexture.h"

namespace Editor {

using namespace Aquila;
using namespace Aquila::Foundation;
using namespace Aquila::GFX;
using namespace Aquila::UI;

namespace {

class ClickableView : public UI::Core::View {
  public:
	Signal<void()> on_click;

	void on_mouse_release(Platform::MouseButton btn, Vec2 pos) override {
		View::on_mouse_release(btn, pos);
		if (btn == Platform::MouseButton::Left) {
			on_click();
		}
	}
};

} // namespace

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

ConsolePanel::ConsolePanel(UI::Core::TextureCache *texture_cache) : m_texture_cache(texture_cache) {
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
	if (m_texture_cache != nullptr) {
		m_info_icon = m_texture_cache->load("Engine/UI/Icons/info.svg");
		m_alert_icon = m_texture_cache->load("Engine/UI/Icons/triangle-alert.svg");
		m_error_icon = m_texture_cache->load("Engine/UI/Icons/circle-x.svg");
	}

	m_scroll_view = panel->find_by_id<UI::Core::ScrollView>("console-scroll");
	if (m_scroll_view != nullptr) {
		m_view = dynamic_cast<UI::Core::SelectableTextView *>(
			m_scroll_view->add_child(std::make_unique<UI::Core::SelectableTextView>()));
	}

	UI::Core::View *toolbar_ptr = panel->find_by_id("console-toolbar");
	if (toolbar_ptr == nullptr) {
		return;
	}

	auto make_filter_btn = [&](Vec4 icon_tint, FilterGroup group, UI::Core::View *&btn_out,
							   UI::Core::Label *&count_out) {
		auto btn = std::make_unique<ClickableView>();
		btn->add_class("console-filter-btn");
		btn_out = btn.get();

		GfxTexture *icon_tex = nullptr;
		switch (group) {
		case FilterGroup::Info:
			icon_tex = m_info_icon;
			break;
		case FilterGroup::Warning:
			icon_tex = m_alert_icon;
			break;
		case FilterGroup::Error:
			icon_tex = m_error_icon;
			break;
		}
		auto icon = std::make_unique<UI::Core::Image>(icon_tex, icon_tint);
		icon->add_class("console-filter-icon");
		btn->add_child(std::move(icon));

		auto count = std::make_unique<UI::Core::Label>("0");
		count->add_class("console-filter-count");
		count_out = dynamic_cast<UI::Core::Label *>(btn->add_child(std::move(count)));

		btn->on_click.connect([this, group]() { toggle_filter(group); });
		toolbar_ptr->add_child(std::move(btn));
	};

	make_filter_btn({ 0.42F, 0.69F, 0.86F, 1.F }, FilterGroup::Info, m_info_filter_btn, m_info_count_label);
	make_filter_btn({ 0.83F, 0.67F, 0.29F, 1.F }, FilterGroup::Warning, m_warning_filter_btn, m_warning_count_label);
	make_filter_btn({ 0.83F, 0.42F, 0.42F, 1.F }, FilterGroup::Error, m_error_filter_btn, m_error_count_label);
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
		LogEntry entry{ level, continuation ? "    " + line : std::move(line) };
		entry.continuation = continuation;
		if (tag_pos != std::string::npos) {
			entry.tag_begin = static_cast<int>(tag_pos);
			entry.tag_length = static_cast<int>(tag.size());
		}
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
	m_view->add_line(entry.message, Vec4(0.F), entry.tag_begin, entry.tag_length, level_tag_color(entry.level));
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

Vec4 ConsolePanel::level_tag_color(LogLevel level) {
	switch (level) {
	case LogLevel::Warning:
		return { 0.83F, 0.67F, 0.29F, 1.F };
	case LogLevel::Error:
		return { 0.87F, 0.45F, 0.45F, 1.F };
	case LogLevel::Critical:
		return { 1.F, 0.32F, 0.32F, 1.F };
	case LogLevel::Debug:
		return { 0.F, 0.8F, 0.8F, 1.F };
	case LogLevel::Trace:
		return { 0.55F, 0.55F, 0.55F, 1.F };
	default:
		return { 0.35F, 0.87F, 0.35F, 1.F };
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

Vec4 ConsolePanel::level_icon_tint(LogLevel level) {
	switch (level) {
	case LogLevel::Warning:
		return { 0.83F, 0.67F, 0.29F, 1.F };
	case LogLevel::Error:
		return { 0.83F, 0.42F, 0.42F, 1.F };
	case LogLevel::Critical:
		return { 1.F, 0.25F, 0.25F, 1.F };
	case LogLevel::Debug:
		return { 0.42F, 0.69F, 0.86F, 1.F };
	case LogLevel::Trace:
		return { 0.53F, 0.53F, 0.53F, 1.F };
	default:
		return { 0.42F, 0.69F, 0.86F, 1.F };
	}
}

} // namespace Editor
