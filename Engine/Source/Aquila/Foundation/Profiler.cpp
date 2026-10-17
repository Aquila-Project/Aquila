

#include "Aquila/Foundation/Profiler.h"

namespace Aquila::Foundation {

void Profiler::begin_frame() {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	m_frame_start = now();
	m_current_depth = 0;
	m_current_frame_entries.clear();
	m_frame_number++;
}

void Profiler::end_frame() {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_frame_duration = elapsed_milliseconds(m_frame_start, now());

	// Sort by startTime so parents appear before their children throughout the rest of EndFrame,
	// PrintLastFrame, and the stored frame history.
	std::ranges::sort(m_current_frame_entries,
					  [](const ProfilerEntry &a, const ProfilerEntry &b) { return a.start_time < b.start_time; });
	m_frame_count++;
	m_fps = 1000.0 / m_frame_duration;

	m_frame_time_history[m_frame_time_history_index] = static_cast<F32>(m_frame_duration);
	m_frame_time_history_index = (m_frame_time_history_index + 1) % m_frame_time_history.size();

	F64 cpu_time = 0.0;
	for (const auto &entry : m_current_frame_entries) {
		if (entry.depth == 0) {
			cpu_time += entry.duration;
		}
	}

	FrameStats frame_stats;
	frame_stats.frame_duration = m_frame_duration;
	frame_stats.cpu_time = cpu_time;
	frame_stats.frame_number = m_frame_number;
	frame_stats.timestamp = now();

	m_frame_stats_history.push_back(frame_stats);
	if (m_frame_stats_history.size() > M_MAX_HISTORY_FRAMES) {
		m_frame_stats_history.erase(m_frame_stats_history.begin());
	}

	for (auto &entry : m_current_frame_entries) {
		auto &stats = m_stats[entry.name];
		stats.name = entry.name;
		stats.total_duration += entry.duration;
		stats.min_duration = std::min(stats.min_duration, entry.duration);
		stats.max_duration = std::max(stats.max_duration, entry.duration);
		stats.frame_count++;
		stats.avg_duration = stats.total_duration / stats.frame_count;
		stats.call_count += entry.call_count;
		stats.depth = entry.depth;
		stats.color = entry.color;
		stats.thread_id = entry.thread_id;

		stats.recent_durations[stats.history_index] = static_cast<F32>(entry.duration);
		stats.history_index = (stats.history_index + 1) % stats.recent_durations.size();
	}

	m_frame_history.push_back(m_current_frame_entries);
	if (m_frame_history.size() > M_MAX_HISTORY_FRAMES) {
		m_frame_history.erase(m_frame_history.begin());
	}

	detect_bottlenecks();
}

void Profiler::begin_section(const std::string &name) {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	ProfilerEntry entry;
	entry.name = name;
	entry.start_time = elapsed_milliseconds(m_frame_start, now());
	entry.depth = m_current_depth;
	entry.call_count = 1;
	entry.thread_id = std::this_thread::get_id();
	entry.color = hash_string(name);

	m_section_stack.push_back(entry);
	m_current_depth++;
}

void Profiler::end_section() {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (m_section_stack.empty()) {
		return;
	}

	auto entry = m_section_stack.back();
	m_section_stack.pop_back();
	m_current_depth--;

	entry.duration = elapsed_milliseconds(m_frame_start, now()) - entry.start_time;
	m_current_frame_entries.push_back(entry);
}

void Profiler::print_frame_summary() const {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	AQUILA_LOG_INFO("=== Frame Profiler Summary ===");
	AQUILA_LOG_INFO("Frame time: {:.3f} ms ({:.1f} FPS)", m_frame_duration, m_fps);
	AQUILA_LOG_INFO("Total frames: {}", m_frame_count);
	AQUILA_LOG_INFO("");

	if (m_frame_history.empty()) {
		return;
	}

	AQUILA_LOG_INFO("{:<50} {:>10} {:>10} {:>10} {:>9}", "Section", "Avg (ms)", "Min (ms)", "Max (ms)", "% Frame");
	AQUILA_LOG_INFO("{:-<93}", "");

	// Walk the last frame in startTime order (sorted in EndFrame) so the tree prints correctly.
	// Look up aggregated stats by name for the avg/min/max columns.
	for (const auto &frame_entry : m_frame_history.back()) {
		auto it = m_stats.find(frame_entry.name);
		if (it == m_stats.end()) {
			continue;
		}
		const ProfilerEntry &stats = it->second;
		std::string label(frame_entry.depth * 2, ' ');
		label += frame_entry.name;
		F64 percentage = m_frame_duration > 0.0 ? (stats.avg_duration / m_frame_duration) * 100.0 : 0.0;
		AQUILA_LOG_INFO("{:<50} {:>10.3f} {:>10.3f} {:>10.3f} {:>8.1f}%", label, stats.avg_duration, stats.min_duration,
						stats.max_duration, percentage);
	}
	AQUILA_LOG_INFO("");

	if (!m_bottlenecks.empty()) {
		AQUILA_LOG_WARNING("=== Detected Bottlenecks ===");
		for (const auto &bottleneck : m_bottlenecks) {
			AQUILA_LOG_WARNING("  - {} ({:.1f}% of frame time)", bottleneck,
							   (m_stats.at(bottleneck).avg_duration / m_frame_duration) * 100.0);
		}
		AQUILA_LOG_INFO("");
	}
}

void Profiler::print_last_frame() const {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (m_current_frame_entries.empty()) {
		return;
	}

	AQUILA_LOG_INFO("=== Last Frame Breakdown ===");
	AQUILA_LOG_INFO("Frame time: {:.3f} ms ({:.1f} FPS)", m_frame_duration, m_fps);
	AQUILA_LOG_INFO("");

	for (const auto &entry : m_current_frame_entries) {
		std::string indent(entry.depth * 2, ' ');
		F64 percentage = (entry.duration / m_frame_duration) * 100.0;
		AQUILA_LOG_INFO("{}{:<35} {:>8.3f} ms ({:>5.1f}%)", indent, entry.name, entry.duration, percentage);
	}
	AQUILA_LOG_INFO("");
}

void Profiler::reset() {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_stats.clear();
	m_frame_history.clear();
	m_frame_stats_history.clear();
	m_current_frame_entries.clear();
	m_section_stack.clear();
	m_bottlenecks.clear();
	m_frame_count = 0;
	m_frame_number = 0;
	m_frame_time_history_index = 0;
	m_frame_time_history.fill(0.0F);
}

F64 Profiler::get_frame_duration() const {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	return m_frame_duration;
}
F64 Profiler::get_fps() const {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	return m_fps;
}
Uint32 Profiler::get_frame_count() const {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	return m_frame_count;
}
Uint32 Profiler::get_frame_number() const {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	return m_frame_number;
}
bool Profiler::is_enabled() const {
	return m_enabled;
}
void Profiler::set_enabled(bool enabled) {
	m_enabled = enabled;
}

const std::vector<ProfilerEntry> &Profiler::get_current_frame_entries() const {
	return m_current_frame_entries;
}
const std::unordered_map<std::string, ProfilerEntry> &Profiler::get_stats() const {
	return m_stats;
}
const std::vector<std::vector<ProfilerEntry>> &Profiler::get_frame_history() const {
	return m_frame_history;
}
const std::vector<FrameStats> &Profiler::get_frame_stats_history() const {
	return m_frame_stats_history;
}
const std::array<F32, 120> &Profiler::get_frame_time_history() const {
	return m_frame_time_history;
}
const std::vector<std::string> &Profiler::get_bottlenecks() const {
	return m_bottlenecks;
}

bool Profiler::get_section_stats(const std::string &name, ProfilerEntry &out) const {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	auto it = m_stats.find(name);
	if (it != m_stats.end()) {
		out = it->second;
		return true;
	}
	return false;
}

Uint32 Profiler::hash_string(const std::string &str) const {
	Uint32 hash = 0x811c9dc5;
	for (char c : str) {
		hash ^= static_cast<Uint32>(c);
		hash *= 0x01000193;
	}
	return hash;
}

void Profiler::detect_bottlenecks() {
	m_bottlenecks.clear();
	constexpr F64 bottleneck_threshold = 0.20;
	for (const auto &[name, entry] : m_stats) {
		if (entry.avg_duration / m_frame_duration > bottleneck_threshold) {
			m_bottlenecks.push_back(name);
		}
	}
}

ProfileSection::ProfileSection(const std::string &name) : m_name(name) {
	if (Profiler::get()->is_enabled()) {
		Profiler::get()->begin_section(m_name);
	}
}

ProfileSection::~ProfileSection() {
	if (Profiler::get()->is_enabled()) {
		Profiler::get()->end_section();
	}
}

} // namespace Aquila::Foundation
