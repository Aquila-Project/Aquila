#ifndef AQUILA_TIMER_H
#define AQUILA_TIMER_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Foundation {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

inline TimePoint now() {
	return Clock::now();
}

inline double elapsed_seconds(const TimePoint &start, const TimePoint &end) {
	return std::chrono::duration<double>(end - start).count();
}

inline double elapsed_milliseconds(const TimePoint &start, const TimePoint &end) {
	return std::chrono::duration<double, std::milli>(end - start).count();
}

inline double get_time_since_start(const TimePoint &start) {
	return elapsed_seconds(start, now());
}

/// Call tick() once per frame, get_delta_time() is the time between the last two ticks in seconds.
class Stopwatch {
  public:
	Stopwatch() { start(); }

	void start() {
		m_start_time = now();
		m_last_frame_time = m_start_time;
		m_delta_time = 0.0F;
	}

	void tick() {
		auto current_time = now();
		m_delta_time = elapsed_seconds(m_last_frame_time, current_time);
		m_last_frame_time = current_time;
	}

	F32 get_delta_time() const { return m_delta_time; }

	F32 get_elapsed_time() const { return elapsed_seconds(m_start_time, now()); }

  private:
	TimePoint m_start_time;
	TimePoint m_last_frame_time;
	F32 m_delta_time = 0.0F;
};

} // namespace Aquila::Foundation

#endif // AQUILA_TIMER_H
