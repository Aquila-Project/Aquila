#pragma once
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/Registration.h"
#include "Aquila/Foundation/Singleton.h"
#include "Aquila/Foundation/Timer.h"

namespace Aquila::Foundation {

/// Nothing renders unless someone asks for it. An idle app renders zero frames and the loop sleeps.
///
/// A request is either a full frame (main window and every secondary window) or a targeted one
/// (one secondary window, the target is its window id).
///
/// @note Never request a frame every frame "just in case", the app will render forever.
/// Compare your inputs and ask only when something changed.
///
/// @code
/// if (value != m_last_value) {
///     m_last_value = value;
///     FrameScheduler::get()->request_frame();
/// }
/// @endcode
class FrameScheduler : public Singleton<FrameScheduler> {
  public:
	/// nullptr means the whole app.
	using Target = const void *;

	/// Safe from any thread. From a worker thread it also wakes the main loop.
	/// Asking many times before the frame runs still gives one frame.
	void request_frame() {
		if (m_dirty.load(std::memory_order_relaxed) || m_dirty.exchange(true)) {
			return;
		}
		if (m_wake && std::this_thread::get_id() != m_owner) {
			m_wake();
		}
	}

	/// @note Main thread only, and it doesn't wake the loop.
	void request_frame(Target target) {
		if (target == nullptr) {
			request_frame();
			return;
		}
		if (std::ranges::find(m_targets, target) == m_targets.end()) {
			m_targets.push_back(target);
		}
	}

	/// For timers like caret blink or tooltip delay. Schedule once and let the loop sleep,
	/// don't request a frame every frame until the time is up.
	///
	/// One timer per target, asking again keeps the earliest one. Main thread only.
	void request_frame_in(F64 seconds, Target target = nullptr) {
		const TimePoint at =
			now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<F64>(std::max(seconds, 0.0)));
		for (Deadline &deadline : m_deadlines) {
			if (deadline.target == target) {
				deadline.at = std::min(deadline.at, at);
				return;
			}
		}
		m_deadlines.push_back({ .target = target, .at = at });
	}

	/// Also true when a timer ran out. The loop sleeps while this is false.
	[[nodiscard]] bool is_pending() const {
		if (m_dirty.load() || !m_targets.empty()) {
			return true;
		}
		const TimePoint current = now();
		return std::ranges::any_of(m_deadlines, [current](const Deadline &deadline) { return deadline.at <= current; });
	}

	/// The loop waits for events at most this long so it wakes up in time for the next timer.
	/// @return 0 if a timer is already late, nullopt when there are no timers
	[[nodiscard]] Option<F64> seconds_until_deadline() const {
		if (m_deadlines.empty()) {
			return std::nullopt;
		}
		const auto earliest = std::ranges::min_element(m_deadlines, {}, &Deadline::at);
		return std::max(elapsed_seconds(now(), earliest->at), 0.0);
	}

	/// Timers that ran out count as requests here.
	bool consume() {
		promote_due_deadlines();
		return m_dirty.exchange(false);
	}

	bool consume(Target target) {
		promote_due_deadlines();
		const auto it = std::ranges::find(m_targets, target);
		if (it == m_targets.end()) {
			return false;
		}
		m_targets.erase(it);
		return true;
	}

	/// The loop calls it after rendering the secondary windows. Without it a request for a window
	/// nobody renders keeps is_pending() true and the loop never sleeps.
	void clear_targets() { m_targets.clear(); }

	/// Call it when a window closes, otherwise its timer still fires.
	void forget(Target target) {
		std::erase(m_targets, target);
		std::erase_if(m_deadlines, [target](const Deadline &deadline) { return deadline.target == target; });
	}

	/// Application passes `Window::wake`. It's a delegate so Foundation doesn't need GLFW.
	void set_wake(Delegate<void()> wake) { m_wake = std::move(wake); }

	/// Runs `poll` every K_POLL_INTERVAL between frames, also while the app is idle, for checks like "is this file
	/// still there". It doesn't render anything: a poll that changes something asks for a frame itself.
	/// Main thread only.
	[[nodiscard]] Registration add_poll(Delegate<void()> poll) {
		const Uint64 id = ++m_last_poll_id;
		m_polls.push_back({ .id = id, .run = std::move(poll) });
		return m_registrations.make([this, id] { std::erase_if(m_polls, [id](const Poll &p) { return p.id == id; }); });
	}

	/// The loop calls it on every pass, the polls only run when they're due. Polls due within K_POLL_SLACK run early,
	/// so they share a wake-up with the loop's other timers instead of waking it again microseconds later.
	/// Polls may add or remove polls.
	void run_polls() {
		const TimePoint current = now();
		if (m_polls.empty() || current + K_POLL_SLACK < m_next_poll) {
			return;
		}
		m_next_poll = current + K_POLL_INTERVAL;
		std::vector<Uint64> ids;
		ids.reserve(m_polls.size());
		for (const Poll &poll : m_polls) {
			ids.push_back(poll.id);
		}
		for (const Uint64 id : ids) {
			const auto it = std::ranges::find(m_polls, id, &Poll::id);
			if (it != m_polls.end()) {
				const Delegate<void()> run = it->run;
				run();
			}
		}
	}

	/// The loop waits for events at most this long so polls keep running while idle.
	/// @return 0 if the polls are due, nullopt when nothing polls
	[[nodiscard]] Option<F64> seconds_until_poll() const {
		if (m_polls.empty()) {
			return std::nullopt;
		}
		return std::max(elapsed_seconds(now(), m_next_poll), 0.0);
	}

	static constexpr std::chrono::milliseconds K_POLL_INTERVAL{ 500 };
	static constexpr std::chrono::milliseconds K_POLL_SLACK{ 5 };

  private:
	friend class Singleton<FrameScheduler>;
	FrameScheduler() = default;

	struct Deadline {
		Target target = nullptr;
		TimePoint at;
	};

	struct Poll {
		Uint64 id = 0;
		Delegate<void()> run;
	};

	void promote_due_deadlines() {
		const TimePoint current = now();
		std::erase_if(m_deadlines, [this, current](const Deadline &deadline) {
			if (deadline.at > current) {
				return false;
			}
			if (deadline.target == nullptr) {
				m_dirty.store(true);
			} else if (std::ranges::find(m_targets, deadline.target) == m_targets.end()) {
				m_targets.push_back(deadline.target);
			}
			return true;
		});
	}

	std::atomic<bool> m_dirty = true; // always render the first frame
	std::vector<Target> m_targets;
	std::vector<Deadline> m_deadlines;
	Delegate<void()> m_wake;
	std::thread::id m_owner = std::this_thread::get_id();
	std::vector<Poll> m_polls;
	Uint64 m_last_poll_id = 0;
	TimePoint m_next_poll{};
	RegistrationOwner m_registrations;
};

} // namespace Aquila::Foundation
