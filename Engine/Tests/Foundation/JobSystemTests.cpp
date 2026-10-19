#include <doctest.h>

#include "Aquila/Foundation/JobSystem.h"

using namespace Aquila;
using Aquila::Foundation::JobPriority;
using Aquila::Foundation::JobSystem;

TEST_SUITE("JobSystem") {
	TEST_CASE("jobs run on the workers and hand their result back") {
		JobSystem::init(4U);
		std::vector<Foundation::JobHandle<int>> handles;
		for (int i = 0; i < 64; ++i) {
			handles.push_back(JobSystem::get()->schedule(JobPriority::Medium, "square", [i] { return i * i; }));
		}
		for (int i = 0; i < 64; ++i) {
			CHECK(handles[static_cast<Usize>(i)].get() == i * i);
		}
		JobSystem::get()->wait_for_all();
		CHECK(JobSystem::get()->get_pending_count() == 0);
		JobSystem::shutdown();
	}

	TEST_CASE("higher priorities run first, equal ones in order") {
		JobSystem::init(1U);
		std::promise<void> release;
		std::shared_future<void> gate = release.get_future().share();
		JobSystem::get()->schedule(JobPriority::Medium, "gate", [gate] { gate.wait(); });

		std::mutex mutex;
		std::vector<std::string> order;
		const auto record = [&](std::string name) {
			return [&, name] {
				std::scoped_lock lock(mutex);
				order.push_back(name);
			};
		};
		JobSystem::get()->schedule(JobPriority::Low, "low", record("low"));
		JobSystem::get()->schedule(JobPriority::Medium, "medium 1", record("medium 1"));
		JobSystem::get()->schedule(JobPriority::High, "high", record("high"));
		JobSystem::get()->schedule(JobPriority::Medium, "medium 2", record("medium 2"));
		release.set_value();
		JobSystem::get()->wait_for_all();

		CHECK(order == std::vector<std::string>{ "high", "medium 1", "medium 2", "low" });
		JobSystem::shutdown();
	}

	TEST_CASE("a job that throws hands the exception to its handle") {
		JobSystem::init(2U);
		auto handle = JobSystem::get()->schedule(JobPriority::High, "broken", []() -> int {
			throw std::runtime_error("nope");
		});
		CHECK_THROWS_AS(handle.get(), std::runtime_error);
		auto fine = JobSystem::get()->schedule(JobPriority::High, "fine", [] { return 7; });
		CHECK(fine.get() == 7);
		JobSystem::shutdown();
	}

	TEST_CASE("shutdown drops queued jobs and joins the running one") {
		JobSystem::init(1U);
		std::promise<void> release;
		std::promise<void> started;
		std::shared_future<void> gate = release.get_future().share();
		std::atomic<int> ran = 0;
		JobSystem::get()->schedule(JobPriority::High, "gate", [gate, &ran, &started] {
			started.set_value();
			gate.wait();
			++ran;
		});
		started.get_future().wait();
		auto dropped = JobSystem::get()->schedule(JobPriority::Low, "dropped", [&ran] { ++ran; });
		std::thread releaser([&release] {
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			release.set_value();
		});
		JobSystem::shutdown();
		releaser.join();
		CHECK(ran == 1);
		CHECK_THROWS_AS(dropped.get(), std::future_error);
	}
}
