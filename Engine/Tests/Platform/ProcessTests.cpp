#include <doctest.h>

#include "Aquila/Platform/Process.h"

using Aquila::Platform::launch_detached;
namespace fs = std::filesystem;

TEST_SUITE("Process") {
	TEST_CASE("launching a missing executable fails") {
		CHECK_FALSE(launch_detached("/definitely/not/here/aquila"));
	}

#ifndef AQUILA_PLATFORM_WINDOWS
	TEST_CASE("a detached process runs with its arguments") {
		const fs::path marker = fs::temp_directory_path() / "aquila_launch_marker";
		fs::remove(marker);
		REQUIRE(launch_detached("/bin/sh", { "-c", "touch '" + marker.string() + "'" }));

		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!fs::exists(marker) && std::chrono::steady_clock::now() < deadline) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		CHECK(fs::exists(marker));
		fs::remove(marker);
	}
#endif
}
