#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include "Aquila/Foundation/Timer.h"
#include "Aquila/Foundation/Log.h"
#include "Aquila/Foundation/Profiler.h"

using namespace Aquila::Foundation;

static void run_frame(const std::string &section = "Work",
					 std::chrono::milliseconds sleep = std::chrono::milliseconds(1)) {
	Profiler::get()->begin_frame();
	Profiler::get()->begin_section(section);
	std::this_thread::sleep_for(sleep);
	Profiler::get()->end_section();
	Profiler::get()->end_frame();
}

struct LogCapture {
	std::ostringstream stream;
	LogLevel previous_level = Logger::get_log_level();

	explicit LogCapture(LogLevel level) {
		Logger::set_sink(&stream);
		Logger::set_log_level(level);
	}

	~LogCapture() {
		Logger::set_sink(nullptr);
		Logger::set_log_level(previous_level);
	}
};

#define RESET() Profiler::get()->reset()
#define PROFILE_INIT() Profiler::init();
#define PROFILE_SHUTDOWN() Profiler::shutdown();

TEST_SUITE("Timer tests") {
	TEST_CASE("now() returns a valid time point") {
		auto before = Clock::now();
		auto current = now();
		auto after = Clock::now();

		CHECK(current >= before);
		CHECK(current <= after);
	}

	TEST_CASE("elapsed_seconds returns correct duration") {
		auto start = now();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		auto end = now();

		double elapsed = elapsed_seconds(start, end);
		CHECK(elapsed >= 0.08);
		CHECK(elapsed <= 0.5);
	}

	TEST_CASE("elapsed_milliseconds returns correct duration") {
		auto start = now();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		auto end = now();

		double elapsed = elapsed_milliseconds(start, end);
		CHECK(elapsed >= 90.0);
		CHECK(elapsed <= 500.0);
	}

	TEST_CASE("elapsed_seconds and elapsed_milliseconds are consistent") {
		auto start = now();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		auto end = now();

		double secs = elapsed_seconds(start, end);
		double millis = elapsed_milliseconds(start, end);

		CHECK(millis == doctest::Approx(secs * 1000.0).epsilon(0.01));
	}

	TEST_CASE("elapsed_seconds with same start and end is ~zero") {
		auto t = now();
		double elapsed = elapsed_seconds(t, t);
		CHECK(elapsed == doctest::Approx(0.0).epsilon(1e-9));
	}

	TEST_CASE("GetTimeSinceStart returns positive elapsed time") {
		auto start = now();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		double elapsed = get_time_since_start(start);
		CHECK(elapsed >= 0.04);
		CHECK(elapsed <= 0.5);
	}

	TEST_CASE("Stopwatch starts on construction") {
		Stopwatch sw;
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		CHECK(sw.get_elapsed_time() >= 0.04f);
	}

	TEST_CASE("Stopwatch::GetElapsedTime increases over time") {
		Stopwatch sw;

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		float t1 = sw.get_elapsed_time();

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		float t2 = sw.get_elapsed_time();

		CHECK(t2 > t1);
	}

	TEST_CASE("Stopwatch::GetDeltaTime is 0 before first Tick") {
		Stopwatch sw;
		CHECK(sw.get_delta_time() == doctest::Approx(0.0f));
	}

	TEST_CASE("Stopwatch::Tick updates delta time") {
		Stopwatch sw;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		sw.tick();

		CHECK(sw.get_delta_time() >= 0.09f);
		CHECK(sw.get_delta_time() <= 0.5f);
	}

	TEST_CASE("Stopwatch::Tick measures time between ticks, not since start") {
		Stopwatch sw;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		sw.tick();

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		sw.tick();

		CHECK(sw.get_delta_time() >= 0.04f);
		CHECK(sw.get_delta_time() <= 0.2f);
	}

	TEST_CASE("Stopwatch::Start resets elapsed time") {
		Stopwatch sw;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		sw.start();
		float elapsed = sw.get_elapsed_time();

		CHECK(elapsed < 0.05f);
	}

	TEST_CASE("Stopwatch::Start resets delta time to 0") {
		Stopwatch sw;
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		sw.tick();

		sw.start();
		CHECK(sw.get_delta_time() == doctest::Approx(0.0f));
	}
}

TEST_SUITE("Logger tests") {
	TEST_CASE("Log debug writes message") {
		LogCapture capture(LogLevel::Debug);
		Logger::log_debug("Test message");
		CHECK(capture.stream.str().find("Test message") != std::string::npos);
	}

	TEST_CASE("Log level filters lower levels") {
		LogCapture capture(LogLevel::Warning);
		Logger::log_debug("Should not appear");
		CHECK(capture.stream.str().empty());
	}

	TEST_CASE("Profiler is enabled by default") {
		PROFILE_INIT();
		RESET();
		CHECK(Profiler::get()->is_enabled());
	}

	TEST_CASE("SetEnabled toggles the flag") {
		RESET();
		Profiler::get()->set_enabled(false);
		CHECK_FALSE(Profiler::get()->is_enabled());
		Profiler::get()->set_enabled(true);
		CHECK(Profiler::get()->is_enabled());
	}

	TEST_CASE("Two profile scopes fit in one block") {
		RESET();
		run_frame();
		{
			PROFILE_SCOPE("First");
			PROFILE_SCOPE("Second");
		}
		ProfilerEntry entry;
		CHECK(Profiler::get()->get_section_stats("First", entry));
		CHECK(Profiler::get()->get_section_stats("Second", entry));
	}

	TEST_CASE("Frame counters start at zero after Reset") {
		RESET();
		CHECK(Profiler::get()->get_frame_count() == 0u);
		CHECK(Profiler::get()->get_frame_number() == 0u);
	}

	TEST_CASE("BeginFrame increments FrameNumber; EndFrame increments FrameCount") {
		RESET();
		run_frame();
		CHECK(Profiler::get()->get_frame_number() == 1u);
		CHECK(Profiler::get()->get_frame_count() == 1u);

		run_frame();
		CHECK(Profiler::get()->get_frame_number() == 2u);
		CHECK(Profiler::get()->get_frame_count() == 2u);
	}

	TEST_CASE("FrameDuration is positive after a frame") {
		RESET();
		run_frame("W", std::chrono::milliseconds(2));
		CHECK(Profiler::get()->get_frame_duration() > 0.0);
	}

	TEST_CASE("FPS is consistent with frame duration (fps * dur ~ 1000 ms)") {
		RESET();
		run_frame("W", std::chrono::milliseconds(10));
		double dur = Profiler::get()->get_frame_duration();
		double fps = Profiler::get()->get_fps();
		CHECK(fps * dur == doctest::Approx(1000.0).epsilon(0.05));
	}

	TEST_CASE("Named section appears in CurrentFrameEntries") {
		RESET();
		Profiler::get()->begin_frame();
		Profiler::get()->begin_section("Render");
		Profiler::get()->end_section();
		Profiler::get()->end_frame();

		const auto &entries = Profiler::get()->get_current_frame_entries();
		REQUIRE(entries.size() == 1u);
		CHECK(entries[0].name == "Render");
	}

	TEST_CASE("Section duration is non-negative") {
		RESET();
		run_frame("Physics");
		const auto &entries = Profiler::get()->get_current_frame_entries();
		REQUIRE_FALSE(entries.empty());
		CHECK(entries[0].duration >= 0.0);
	}

	TEST_CASE("Multiple sections in one frame are all recorded") {
		RESET();
		Profiler::get()->begin_frame();
		for (auto name : { "A", "B", "C" }) {
			Profiler::get()->begin_section(name);
			Profiler::get()->end_section();
		}
		Profiler::get()->end_frame();

		CHECK(Profiler::get()->get_current_frame_entries().size() == 3u);
	}

	TEST_CASE("Nested sections get correct depth values") {
		RESET();
		Profiler::get()->begin_frame();
		Profiler::get()->begin_section("Outer");
		Profiler::get()->begin_section("Inner");
		Profiler::get()->end_section();
		Profiler::get()->end_section();
		Profiler::get()->end_frame();

		const auto &e = Profiler::get()->get_current_frame_entries();
		REQUIRE(e.size() == 2u);
		CHECK(e[0].name == "Outer");
		CHECK(e[0].depth == 0);
		CHECK(e[1].name == "Inner");
		CHECK(e[1].depth == 1);
	}

	TEST_CASE("Stats accumulate correctly over multiple frames") {
		RESET();
		for (int i = 0; i < 5; ++i) {
			run_frame("Loop");
		}

		ProfilerEntry stats;
		REQUIRE(Profiler::get()->get_section_stats("Loop", stats));
		CHECK(stats.frame_count == 5u);
		CHECK(stats.avg_duration >= 0.0);
		CHECK(stats.min_duration <= stats.max_duration);
	}

	TEST_CASE("GetSectionStats returns false for unknown section") {
		RESET();
		ProfilerEntry dummy;
		CHECK_FALSE(Profiler::get()->get_section_stats("DoesNotExist", dummy));
	}

	TEST_CASE("FrameHistory grows by one entry per frame") {
		RESET();
		run_frame();
		run_frame();
		run_frame();
		CHECK(Profiler::get()->get_frame_history().size() == 3u);
	}

	TEST_CASE("FrameStatsHistory size matches FrameHistory size") {
		RESET();
		for (int i = 0; i < 7; ++i) {
			run_frame();
		}
		CHECK(Profiler::get()->get_frame_stats_history().size() == Profiler::get()->get_frame_history().size());
	}

	TEST_CASE("FrameTimeHistory ring buffer is 120 elements") {
		RESET();
		CHECK(Profiler::get()->get_frame_time_history().size() == 120u);
	}

	TEST_CASE("FrameTimeHistory records non-zero values after frames") {
		RESET();
		for (int i = 0; i < 3; ++i) {
			run_frame("W", std::chrono::milliseconds(1));
		}

		int nonzero = 0;
		for (float v : Profiler::get()->get_frame_time_history()) {
			if (v > 0.0f) {
				++nonzero;
			}
		}
		CHECK(nonzero == 3);
	}

	TEST_CASE("Reset clears all accumulated state") {
		RESET();
		for (int i = 0; i < 10; ++i) {
			run_frame("X");
		}

		Profiler::get()->reset();

		CHECK(Profiler::get()->get_frame_count() == 0u);
		CHECK(Profiler::get()->get_frame_number() == 0u);
		CHECK(Profiler::get()->get_stats().empty());
		CHECK(Profiler::get()->get_frame_history().empty());
		CHECK(Profiler::get()->get_frame_stats_history().empty());
		CHECK(Profiler::get()->get_current_frame_entries().empty());
		CHECK(Profiler::get()->get_bottlenecks().empty());

		for (float v : Profiler::get()->get_frame_time_history()) {
			CHECK(v == doctest::Approx(0.0f));
		}
	}

	TEST_CASE("EndSection on empty stack does not crash") {
		RESET();
		Profiler::get()->begin_frame();
		CHECK_NOTHROW(Profiler::get()->end_section());
		Profiler::get()->end_frame();
	}

	TEST_CASE("Section consuming >20% of frame time is flagged as bottleneck") {
		RESET();
		for (int i = 0; i < 5; ++i) {
			Profiler::get()->begin_frame();
			Profiler::get()->begin_section("HeavyWork");
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			Profiler::get()->end_section();
			Profiler::get()->end_frame();
		}

		const auto &bns = Profiler::get()->get_bottlenecks();
		bool found = std::find(bns.begin(), bns.end(), "HeavyWork") != bns.end();
		CHECK(found);
	}

	TEST_CASE("ProfileSection RAII calls Begin/End automatically") {
		RESET();
		Profiler::get()->begin_frame();
		{
			ProfileSection ps("RAIISection");
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		Profiler::get()->end_frame();

		const auto &entries = Profiler::get()->get_current_frame_entries();
		REQUIRE(entries.size() == 1u);
		CHECK(entries[0].name == "RAIISection");
		CHECK(entries[0].duration > 0.0);
	}

	TEST_CASE("ProfileSection does nothing when profiler is disabled") {
		RESET();
		Profiler::get()->set_enabled(false);

		Profiler::get()->begin_frame();
		{
			ProfileSection ps("Ignored");
		}
		Profiler::get()->end_frame();

		CHECK(Profiler::get()->get_current_frame_entries().empty());
		Profiler::get()->set_enabled(true);
	}

	TEST_CASE("Section records the calling thread id") {
		RESET();
		run_frame("TID");
		const auto &e = Profiler::get()->get_current_frame_entries();
		REQUIRE_FALSE(e.empty());
		CHECK(e[0].thread_id == std::this_thread::get_id());
	}

	TEST_CASE("Same section name always produces the same color hash") {
		RESET();
		run_frame("Stable");
		auto color1 = Profiler::get()->get_current_frame_entries()[0].color;

		RESET();
		run_frame("Stable");
		auto color2 = Profiler::get()->get_current_frame_entries()[0].color;

		CHECK(color1 == color2);
	}

	TEST_CASE("Different section names produce different color hashes") {
		RESET();
		Profiler::get()->begin_frame();
		Profiler::get()->begin_section("Alpha");
		Profiler::get()->end_section();
		Profiler::get()->begin_section("Beta");
		Profiler::get()->end_section();
		Profiler::get()->end_frame();

		const auto &e = Profiler::get()->get_current_frame_entries();
		REQUIRE(e.size() == 2u);
		CHECK(e[0].color != e[1].color);

		PROFILE_SHUTDOWN(); // ! TODO: THIS SHOULD ALWAYS BE IN THE LAST TEST FOR THE PROFILER
	}
}
