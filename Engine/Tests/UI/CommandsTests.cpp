#include <doctest.h>

#include "Aquila/UI/Core/Command.h"

using namespace Aquila::UI::Core;

TEST_SUITE("Commands") {
	TEST_CASE("a command without a predicate is enabled and runs") {
		Commands commands;
		int count = 0;
		commands.add({ .id = "editor.quit", .run = [&count] { ++count; } });
		CHECK(commands.is_enabled("editor.quit"));
		CHECK(commands.execute("editor.quit"));
		CHECK(count == 1);
	}

	TEST_CASE("the predicate decides whether it runs") {
		Commands commands;
		int count = 0;
		bool editing = false;
		commands.add({ .id = "scene.save", .run = [&count] { ++count; }, .enabled = [&editing] { return editing; } });
		CHECK_FALSE(commands.is_enabled("scene.save"));
		CHECK_FALSE(commands.execute("scene.save"));
		editing = true;
		CHECK(commands.execute("scene.save"));
		CHECK(count == 1);
	}

	TEST_CASE("unknown ids and commands without run do nothing") {
		Commands commands;
		commands.add({ .id = "empty" });
		CHECK_FALSE(commands.is_enabled("missing"));
		CHECK_FALSE(commands.execute("missing"));
		CHECK_FALSE(commands.is_enabled("empty"));
		CHECK_FALSE(commands.execute("empty"));
	}

	TEST_CASE("adding an id twice keeps one command, the last one") {
		Commands commands;
		int first = 0;
		int second = 0;
		commands.add({ .id = "a", .run = [&first] { ++first; } });
		commands.add({ .id = "a", .run = [&second] { ++second; } });
		CHECK(commands.all().size() == 1);
		CHECK(commands.execute("a"));
		CHECK(first == 0);
		CHECK(second == 1);
	}

	TEST_CASE("a command may remove itself while it runs") {
		Commands commands;
		int count = 0;
		commands.add({ .id = "once", .run = [&commands, &count] {
						  ++count;
						  commands.remove("once");
					  } });
		CHECK(commands.execute("once"));
		CHECK(count == 1);
		CHECK(commands.find("once") == nullptr);
	}
}
