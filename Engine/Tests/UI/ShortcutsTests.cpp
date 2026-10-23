#include <doctest.h>

#include "Aquila/UI/Core/Shortcuts.h"

using namespace Aquila::UI::Core;
using Aquila::Platform::Events::KeyChord;

namespace {

KeyChord chord(std::string_view text) {
	return *KeyChord::parse(text);
}

Command counter(std::string id, int &count) {
	return { .id = std::move(id), .title = "Counter", .run = [&count] { ++count; } };
}

}

TEST_SUITE("Shortcuts") {
	TEST_CASE("a bound chord runs its command") {
		Commands commands;
		Shortcuts shortcuts{ commands };
		int count = 0;
		commands.add(counter("scene.new", count));
		shortcuts.bind("scene.new", "Ctrl+N");

		CHECK(shortcuts.handle(chord("Ctrl+N"), false, false));
		CHECK(count == 1);
		CHECK_FALSE(shortcuts.handle(chord("Ctrl+M"), false, false));
		CHECK(count == 1);
	}

	TEST_CASE("scoped bindings need an active scope and win over global ones") {
		Commands commands;
		Shortcuts shortcuts{ commands };
		int viewport = 0;
		int global = 0;
		bool hovered = false;
		commands.add(counter("transform.translate", viewport));
		commands.add(counter("window.close", global));
		shortcuts.bind("window.close", "W");
		shortcuts.bind("transform.translate", "W", "viewport");
		shortcuts.set_scope_resolver([&hovered](std::string_view scope) { return scope == "viewport" && hovered; });

		CHECK(shortcuts.handle(chord("W"), false, false));
		CHECK(global == 1);
		CHECK(viewport == 0);

		hovered = true;
		CHECK(shortcuts.handle(chord("W"), false, false));
		CHECK(viewport == 1);
		CHECK(global == 1);
	}

	TEST_CASE("a disabled command lets the key fall through") {
		Commands commands;
		Shortcuts shortcuts{ commands };
		int count = 0;
		commands.add({ .id = "transform.confirm", .run = [&count] { ++count; }, .enabled = [] { return false; } });
		shortcuts.bind("transform.confirm", "Enter");
		CHECK_FALSE(shortcuts.handle(chord("Enter"), false, false));
		CHECK(count == 0);
		CHECK_FALSE(commands.execute("transform.confirm"));
	}

	TEST_CASE("typing only lets modified and function keys through") {
		Commands commands;
		Shortcuts shortcuts{ commands };
		int count = 0;
		commands.add(counter("a", count));
		shortcuts.bind("a", "W");
		shortcuts.bind("a", "Ctrl+N");
		shortcuts.bind("a", "F5");
		shortcuts.bind("a", "Ctrl+C");

		CHECK_FALSE(shortcuts.handle(chord("W"), false, true));
		CHECK_FALSE(shortcuts.handle(chord("Ctrl+C"), false, true));
		CHECK(shortcuts.handle(chord("Ctrl+N"), false, true));
		CHECK(shortcuts.handle(chord("F5"), false, true));
		CHECK(count == 2);
	}

	TEST_CASE("held keys only repeat repeatable commands") {
		Commands commands;
		Shortcuts shortcuts{ commands };
		int toggles = 0;
		int zooms = 0;
		commands.add(counter("toggle", toggles));
		commands.add({ .id = "zoom", .run = [&zooms] { ++zooms; }, .repeatable = true });
		shortcuts.bind("toggle", "T");
		shortcuts.bind("zoom", "Ctrl+=");

		CHECK(shortcuts.handle(chord("T"), true, false));
		CHECK(toggles == 0);
		CHECK(shortcuts.handle(chord("Ctrl+="), true, false));
		CHECK(zooms == 1);
	}

	TEST_CASE("user chords replace the defaults and keep their scope") {
		Commands commands;
		Shortcuts shortcuts{ commands };
		int count = 0;
		commands.add(counter("entity.delete", count));
		shortcuts.bind("entity.delete", "Delete", "viewport");
		shortcuts.set_scope_resolver([](std::string_view scope) { return scope == "viewport"; });

		shortcuts.set_chords("entity.delete", { chord("X") });
		CHECK_FALSE(shortcuts.handle(chord("Delete"), false, false));
		CHECK(shortcuts.handle(chord("X"), false, false));
		CHECK(count == 1);
		REQUIRE(shortcuts.get_bindings("entity.delete").size() == 1);
		CHECK(shortcuts.get_bindings("entity.delete")[0].scope == "viewport");
		CHECK(shortcuts.get_label("entity.delete") == "X");

		shortcuts.set_chords("entity.delete", {});
		CHECK_FALSE(shortcuts.handle(chord("X"), false, false));
		CHECK(shortcuts.get_label("entity.delete").empty());
	}

	TEST_CASE("removed commands leave their bindings inert") {
		Commands commands;
		Shortcuts shortcuts{ commands };
		int count = 0;
		commands.add(counter("console.clear", count));
		shortcuts.bind("console.clear", "Ctrl+L");
		commands.remove("console.clear");
		CHECK_FALSE(shortcuts.handle(chord("Ctrl+L"), false, false));
		CHECK(commands.find("console.clear") == nullptr);
	}

	TEST_CASE("a command may register more commands while it runs") {
		Commands commands;
		Shortcuts shortcuts{ commands };
		int added = 0;
		commands.add({ .id = "window.open", .run = [&commands, &added] {
						  for (int i = 0; i < 64; ++i) {
							  commands.add({ .id = "generated." + std::to_string(added++), .run = [] {} });
						  }
					  } });
		shortcuts.bind("window.open", "Ctrl+O");
		CHECK(shortcuts.handle(chord("Ctrl+O"), false, false));
		CHECK(commands.all().size() == 65);
	}
}
