#include <doctest.h>

#include "Aquila/Foundation/FrameScheduler.h"

using Aquila::Foundation::FrameScheduler;
using Aquila::Foundation::Registration;

namespace {

struct SchedulerScope {
	SchedulerScope() {
		FrameScheduler::init();
		FrameScheduler::get()->consume();
	}
	~SchedulerScope() { FrameScheduler::shutdown(); }
};

}

TEST_SUITE("FrameScheduler") {
	TEST_CASE("the first frame is pending right after init") {
		FrameScheduler::init();
		CHECK(FrameScheduler::get()->is_pending());
		CHECK(FrameScheduler::get()->consume());
		CHECK_FALSE(FrameScheduler::get()->is_pending());
		FrameScheduler::shutdown();
	}

	TEST_CASE("a request is consumed exactly once") {
		SchedulerScope scope;
		FrameScheduler *scheduler = FrameScheduler::get();
		scheduler->request_frame();
		scheduler->request_frame();
		CHECK(scheduler->is_pending());
		CHECK(scheduler->consume());
		CHECK_FALSE(scheduler->consume());
	}

	TEST_CASE("a timed request only becomes pending once it is due") {
		SchedulerScope scope;
		FrameScheduler *scheduler = FrameScheduler::get();
		CHECK_FALSE(scheduler->seconds_until_deadline().has_value());

		scheduler->request_frame_in(0.05);
		CHECK_FALSE(scheduler->is_pending());
		CHECK_FALSE(scheduler->consume());
		REQUIRE(scheduler->seconds_until_deadline().has_value());
		CHECK(*scheduler->seconds_until_deadline() <= 0.05);

		std::this_thread::sleep_for(std::chrono::milliseconds(70));
		CHECK(scheduler->is_pending());
		CHECK(scheduler->consume());
		CHECK_FALSE(scheduler->seconds_until_deadline().has_value());
		CHECK_FALSE(scheduler->consume());
	}

	TEST_CASE("the earliest timed request wins") {
		SchedulerScope scope;
		FrameScheduler *scheduler = FrameScheduler::get();
		scheduler->request_frame_in(10.0);
		scheduler->request_frame_in(0.5);
		scheduler->request_frame_in(5.0);
		REQUIRE(scheduler->seconds_until_deadline().has_value());
		CHECK(*scheduler->seconds_until_deadline() <= 0.5);
	}

	TEST_CASE("a targeted request renders only its target") {
		SchedulerScope scope;
		FrameScheduler *scheduler = FrameScheduler::get();
		int window = 0;
		int other = 0;
		scheduler->request_frame(&window);
		CHECK(scheduler->is_pending());
		CHECK_FALSE(scheduler->consume());
		CHECK_FALSE(scheduler->consume(&other));
		CHECK(scheduler->consume(&window));
		CHECK_FALSE(scheduler->consume(&window));
		CHECK_FALSE(scheduler->is_pending());
	}

	TEST_CASE("a null target is a full frame") {
		SchedulerScope scope;
		FrameScheduler *scheduler = FrameScheduler::get();
		scheduler->request_frame(nullptr);
		CHECK(scheduler->consume());
	}

	TEST_CASE("a targeted timer wakes only its target") {
		SchedulerScope scope;
		FrameScheduler *scheduler = FrameScheduler::get();
		int window = 0;
		scheduler->request_frame_in(0.0, &window);
		CHECK(scheduler->is_pending());
		CHECK_FALSE(scheduler->consume());
		CHECK(scheduler->consume(&window));
		CHECK_FALSE(scheduler->is_pending());
	}

	TEST_CASE("forgetting a target drops its requests and timers") {
		SchedulerScope scope;
		FrameScheduler *scheduler = FrameScheduler::get();
		int window = 0;
		scheduler->request_frame(&window);
		scheduler->request_frame_in(10.0, &window);
		scheduler->forget(&window);
		CHECK_FALSE(scheduler->is_pending());
		CHECK_FALSE(scheduler->seconds_until_deadline().has_value());
	}

	TEST_CASE("requests on the owning thread never wake the loop") {
		SchedulerScope scope;
		FrameScheduler *scheduler = FrameScheduler::get();
		int wakes = 0;
		scheduler->set_wake([&wakes] { ++wakes; });
		scheduler->request_frame();
		CHECK(wakes == 0);
	}

	TEST_CASE("requests from another thread wake the loop once per frame") {
		SchedulerScope scope;
		FrameScheduler *scheduler = FrameScheduler::get();
		std::atomic<int> wakes = 0;
		scheduler->set_wake([&wakes] { ++wakes; });

		std::thread worker([scheduler] {
			scheduler->request_frame();
			scheduler->request_frame();
		});
		worker.join();
		CHECK(wakes == 1);
		CHECK(scheduler->consume());

		std::thread second([scheduler] { scheduler->request_frame(); });
		second.join();
		CHECK(wakes == 2);
	}

	TEST_CASE("polls run right away, then once per interval, without asking for frames") {
		SchedulerScope scope;
		FrameScheduler *scheduler = FrameScheduler::get();
		CHECK_FALSE(scheduler->seconds_until_poll().has_value());

		int runs = 0;
		Registration poll = scheduler->add_poll([&runs] { ++runs; });
		REQUIRE(scheduler->seconds_until_poll().has_value());
		CHECK(*scheduler->seconds_until_poll() == 0.0);

		scheduler->run_polls();
		CHECK(runs == 1);
		scheduler->run_polls();
		CHECK(runs == 1);
		CHECK(*scheduler->seconds_until_poll() > 0.4);
		CHECK_FALSE(scheduler->is_pending());

		poll.reset();
		CHECK_FALSE(scheduler->seconds_until_poll().has_value());
	}

	TEST_CASE("a poll can remove another poll while they run") {
		SchedulerScope scope;
		FrameScheduler *scheduler = FrameScheduler::get();
		Registration second;
		int second_runs = 0;
		const Registration first = scheduler->add_poll([&second] { second.reset(); });
		second = scheduler->add_poll([&second_runs] { ++second_runs; });

		scheduler->run_polls();
		CHECK(second_runs == 0);
		CHECK_FALSE(second.is_active());
	}

	TEST_CASE("a poll registration outliving the scheduler does nothing") {
		Registration poll;
		{
			SchedulerScope scope;
			poll = FrameScheduler::get()->add_poll([] {});
		}
		poll.reset();
		CHECK_FALSE(poll.is_active());
	}
}
