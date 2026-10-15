#ifndef AQUILA_PROFILER_H
#define AQUILA_PROFILER_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/Singleton.h"
#include "Aquila/Foundation/Timer.h"

namespace Aquila::Foundation {

struct ProfilerEntry {
	std::string name;
	F64 start_time = 0.0;
	F64 duration = 0.0;
	Uint32 depth = 0;
	Uint32 call_count = 0;
	std::thread::id thread_id;
	Uint32 color = 0;

	F64 avg_duration = 0.0;
	F64 min_duration = DBL_MAX;
	F64 max_duration = 0.0;
	F64 total_duration = 0.0;
	Uint32 frame_count = 0;

	std::array<F32, 60> recent_durations = {};
	Uint32 history_index = 0;

	ProfilerEntry() { recent_durations.fill(0.0F); }
};

struct FrameStats {
	F64 frame_duration = 0.0;
	F64 cpu_time = 0.0;
	F64 gpu_time = 0.0;
	Uint32 frame_number = 0;
	TimePoint timestamp;
};

class Profiler : public Singleton<Profiler> {
	friend class Singleton<Profiler>;

  public:
	void begin_frame();
	void end_frame();
	void begin_section(const std::string &name);
	void end_section();
	void print_frame_summary() const;
	void print_last_frame() const;
	void reset();

	F64 get_frame_duration() const;
	F64 get_fps() const;
	Uint32 get_frame_count() const;
	Uint32 get_frame_number() const;
	bool is_enabled() const;
	void set_enabled(bool enabled);

	bool get_section_stats(const std::string &name, ProfilerEntry &out) const;

	const std::vector<ProfilerEntry> &get_current_frame_entries() const;
	const std::unordered_map<std::string, ProfilerEntry> &get_stats() const;
	const std::vector<std::vector<ProfilerEntry>> &get_frame_history() const;
	const std::vector<FrameStats> &get_frame_stats_history() const;
	const std::array<F32, 120> &get_frame_time_history() const;
	const std::vector<std::string> &get_bottlenecks() const;

  private:
	Profiler() { m_frame_time_history.fill(0.0F); }

	Uint32 hash_string(const std::string &str) const;
	void detect_bottlenecks();

	mutable std::recursive_mutex m_mutex;
	bool m_enabled = true;

	TimePoint m_frame_start;
	F64 m_frame_duration = 0.0;
	F64 m_fps = 0.0;
	Uint32 m_current_depth = 0;
	Uint32 m_frame_count = 0;
	Uint32 m_frame_number = 0;
	size_t m_frame_time_history_index = 0;

	std::vector<ProfilerEntry> m_section_stack;
	std::vector<ProfilerEntry> m_current_frame_entries;
	std::unordered_map<std::string, ProfilerEntry> m_stats;
	std::vector<std::vector<ProfilerEntry>> m_frame_history;
	std::vector<FrameStats> m_frame_stats_history;
	std::vector<std::string> m_bottlenecks;
	std::array<F32, 120> m_frame_time_history{};

	static constexpr size_t M_MAX_HISTORY_FRAMES = 60;
};

class ProfileSection {
  public:
	explicit ProfileSection(const std::string &name);
	~ProfileSection();

  private:
	std::string m_name;
};

} // namespace Aquila::Foundation

#define PROFILE_FRAME_BEGIN() Aquila::Foundation::Profiler::get()->begin_frame()
#define PROFILE_FRAME_END() Aquila::Foundation::Profiler::get()->end_frame()
#define AQUILA_PROFILE_CONCAT_INNER(a, b) a##b
#define AQUILA_PROFILE_CONCAT(a, b) AQUILA_PROFILE_CONCAT_INNER(a, b)
#define PROFILE_SCOPE(name) Aquila::Foundation::ProfileSection AQUILA_PROFILE_CONCAT(_profile_, __LINE__)(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)
#define PROFILE_PRINT_SUMMARY_EVERY_N_FRAMES(n)                                   \
	do {                                                                          \
		if (Aquila::Foundation::Profiler::get()->get_frame_number() % (n) == 0) { \
			Aquila::Foundation::Profiler::get()->print_frame_summary();           \
		}                                                                         \
	} while (0)
#endif
