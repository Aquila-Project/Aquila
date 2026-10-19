#include "Aquila/Foundation/JobSystem.h"
#include "Aquila/Foundation/Macros.h"

namespace Aquila::Foundation {

namespace {

void lower_thread_priority() {
#ifdef _WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#else
	[[maybe_unused]] const int result = nice(5);
#endif
}

}

JobSystem::JobSystem(Uint32 thread_count) {
	if (thread_count == 0) {
		const Uint32 cores = std::thread::hardware_concurrency();
		thread_count = cores > 1 ? cores - 1 : 1;
	}
	m_workers.reserve(thread_count);
	for (Uint32 i = 0; i < thread_count; ++i) {
		m_workers.emplace_back([this] {
			lower_thread_priority();
			worker_loop();
		});
	}
	AQUILA_LOG_INFO("JobSystem: {} worker threads", thread_count);
}

JobSystem::~JobSystem() {
	{
		std::scoped_lock lock(m_mutex);
		m_stopping = true;
		m_queue = {};
	}
	m_wake.notify_all();
	for (std::thread &worker : m_workers) {
		if (worker.joinable()) {
			worker.join();
		}
	}
}

void JobSystem::push(Job job) {
	{
		std::scoped_lock lock(m_mutex);
		if (m_stopping) {
			return;
		}
		job.order = m_next_order++;
		m_queue.push(std::move(job));
	}
	m_wake.notify_one();
}

void JobSystem::wait_for_all() {
	std::unique_lock lock(m_mutex);
	m_idle.wait(lock, [this] { return m_queue.empty() && m_running == 0; });
}

Usize JobSystem::get_pending_count() const {
	std::scoped_lock lock(m_mutex);
	return m_queue.size() + m_running;
}

void JobSystem::log_failure(const std::string &debug_name, const char *what) {
	AQUILA_LOG_ERROR("JobSystem: job '{}' failed: {}", debug_name, what);
}

void JobSystem::worker_loop() {
	while (true) {
		Job job;
		{
			std::unique_lock lock(m_mutex);
			m_wake.wait(lock, [this] { return m_stopping || !m_queue.empty(); });
			if (m_stopping) {
				return;
			}
			job = m_queue.top();
			m_queue.pop();
			++m_running;
		}

		job.task();

		{
			std::scoped_lock lock(m_mutex);
			--m_running;
			if (m_queue.empty() && m_running == 0) {
				m_idle.notify_all();
			}
		}
	}
}

}
