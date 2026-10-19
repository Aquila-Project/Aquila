#ifndef AQUILA_FOUNDATION_JOB_SYSTEM_H
#define AQUILA_FOUNDATION_JOB_SYSTEM_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/Singleton.h"

namespace Aquila::Foundation {

/// Higher runs first. Jobs of the same priority run in the order they were scheduled.
enum class JobPriority : Uint8 { Low, Medium, High };

/// Waits on or reads a scheduled job's result. Copyable, every copy sees the same job.
template <typename T> class JobHandle {
  public:
	JobHandle() = default;
	explicit JobHandle(std::shared_future<T> future) : m_future(std::move(future)) {}

	[[nodiscard]] bool is_valid() const { return m_future.valid(); }
	[[nodiscard]] bool is_complete() const {
		return m_future.valid() && m_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
	}
	void wait() const {
		if (m_future.valid()) {
			m_future.wait();
		}
	}
	/// Blocks until the job ran. Rethrows what the job threw, std::future_error when it was dropped by shutdown().
	decltype(auto) get() const { return m_future.get(); }

  private:
	std::shared_future<T> m_future;
};

/// Worker threads for work that doesn't touch the GPU or the scene: decoding files, building mips, baking data.
/// Workers run at a lower OS priority than the main thread so the editor stays responsive while they're busy.
///
/// Jobs must not call into the GPU (GfxContext is main thread only): hand the result back, usually through a queue
/// the main thread drains, and call FrameScheduler::request_frame() so it wakes up to do it.
///
/// @code
/// JobHandle<Image> image = JobSystem::get()->schedule(JobPriority::Medium, "decode", [path] { return decode(path); });
/// @endcode
class JobSystem : public Singleton<JobSystem> {
  public:
	/// 0 threads = one per core minus one for the main thread.
	explicit JobSystem(Uint32 thread_count = 0);
	~JobSystem();

	AQUILA_NONCOPYABLE(JobSystem);
	AQUILA_NONMOVEABLE(JobSystem);

	/// Thread safe. A job that throws is logged with `debug_name`, the handle rethrows it.
	template <typename Func>
	auto schedule(JobPriority priority, std::string debug_name, Func &&func) -> JobHandle<std::invoke_result_t<Func>> {
		using Result = std::invoke_result_t<Func>;
		auto task = std::make_shared<std::packaged_task<Result()>>(
			[func = std::forward<Func>(func), name = debug_name]() mutable -> Result {
				try {
					return func();
				} catch (const std::exception &error) {
					log_failure(name, error.what());
					throw;
				}
			});
		JobHandle<Result> handle(task->get_future().share());
		push({ .task = [task] { (*task)(); }, .debug_name = std::move(debug_name), .priority = priority });
		return handle;
	}

	/// Blocks until nothing is queued or running. Main thread only, never from inside a job.
	void wait_for_all();

	[[nodiscard]] Usize get_pending_count() const;
	[[nodiscard]] Uint32 get_thread_count() const { return static_cast<Uint32>(m_workers.size()); }

  private:
	struct Job {
		std::function<void()> task;
		std::string debug_name;
		JobPriority priority = JobPriority::Medium;
		Uint64 order = 0;
	};

	struct JobOrder {
		bool operator()(const Job &a, const Job &b) const {
			return a.priority != b.priority ? a.priority < b.priority : a.order > b.order;
		}
	};

	static void log_failure(const std::string &debug_name, const char *what);
	void push(Job job);
	void worker_loop();

	mutable std::mutex m_mutex;
	std::condition_variable m_wake;
	std::condition_variable m_idle;
	std::priority_queue<Job, std::vector<Job>, JobOrder> m_queue;
	std::vector<std::thread> m_workers;
	Uint64 m_next_order = 0;
	Uint32 m_running = 0;
	bool m_stopping = false;
};

}

#endif
