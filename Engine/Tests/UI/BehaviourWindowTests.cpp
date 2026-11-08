#include <doctest.h>

#include "Aquila/Platform/Events/InputEvent.h"
#include "Aquila/UI/Core/BehaviourWindow.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Widgets/Label.h"
#include "CanvasTestSupport.h"
#include "VfsTestSupport.h"

using namespace Aquila;
using namespace Aquila::UI::Core;

namespace {

const std::string K_FILE = std::string(MarkupFiles::K_MOUNT) + "/window.aqlayout";

struct CountingWindow final : WindowBehaviour {
	CountingWindow(std::string path, int &built) : WindowBehaviour(std::move(path)), built(built) {}

	void define_behaviour() override {
		++built;
		if (auto *title = get<Label>("title")) {
			title->set_text(std::format("{} {}", title->get_text(), carried));
		}
	}

	void take_state(WindowBehaviour &previous) override {
		carried = dynamic_cast<CountingWindow &>(previous).carried + 1;
	}

	bool intercept_event(Platform::Events::Event &) override {
		++intercepted;
		return true;
	}

	int &built;
	int carried = 0;
	int intercepted = 0;
};

std::string title_of(const BehaviourWindow &window) {
	const auto *title = window.get_canvas()->get_root()->find_by_id<Label>("title");
	return title != nullptr ? title->get_text() : std::string("<none>");
}

void write_window(const MarkupFiles &files, std::string_view title) {
	files.write(K_FILE, std::format(R"(<View id="root"><Label id="title">{}</Label></View>)", title));
}

}

TEST_SUITE("BehaviourWindow") {
	TEST_CASE("rebuild reads the markup again and the new behaviour takes over the old one's state") {
		CanvasSingletonsScope singletons;
		MarkupFiles files;
		write_window(files, "First");
		LayoutLoader loader;
		int built = 0;
		BehaviourWindow window(loader, [&built] { return std::make_unique<CountingWindow>(K_FILE, built); }, 200, 100);
		CHECK(title_of(window) == "First 0");

		write_window(files, "Fresh");
		loader.forget_documents();
		CHECK(window.rebuild());
		CHECK(title_of(window) == "Fresh 1");
		CHECK(built == 2);
		CHECK(window.get_canvas()->get_root()->get_children().size() == 1);
		REQUIRE(window.get_behaviour<CountingWindow>() != nullptr);
		CHECK(window.get_behaviour<CountingWindow>()->carried == 1);
	}

	TEST_CASE("a window made from a behaviour can't rebuild") {
		CanvasSingletonsScope singletons;
		MarkupFiles files;
		write_window(files, "Only");
		LayoutLoader loader;
		int built = 0;
		BehaviourWindow window(loader, std::make_unique<CountingWindow>(K_FILE, built), 200, 100);
		CHECK_FALSE(window.rebuild());
		CHECK(title_of(window) == "Only 0");
		CHECK(built == 1);
	}

	TEST_CASE("markup that doesn't build leaves the window empty until it does") {
		CanvasSingletonsScope singletons;
		MarkupFiles files;
		write_window(files, "Before");
		LayoutLoader loader;
		int built = 0;
		BehaviourWindow window(loader, [&built] { return std::make_unique<CountingWindow>(K_FILE, built); }, 200, 100);

		std::filesystem::remove(std::filesystem::path(test_vfs().to_native_path(K_FILE).value()));
		loader.forget_documents();
		{
			LogCapture capture;
			CHECK_FALSE(window.rebuild());
		}
		CHECK(window.get_canvas()->get_root()->get_children().empty());
		CHECK(window.get_behaviour<CountingWindow>() == nullptr);

		write_window(files, "After");
		loader.forget_documents();
		CHECK(window.rebuild());
		CHECK(title_of(window) == "After 0");
	}

	TEST_CASE("the event hook sees events before the behaviour and can swallow them") {
		CanvasSingletonsScope singletons;
		MarkupFiles files;
		write_window(files, "Keys");
		LayoutLoader loader;
		int built = 0;
		BehaviourWindow window(loader, [&built] { return std::make_unique<CountingWindow>(K_FILE, built); }, 200, 100);
		int hooked = 0;
		bool swallow = true;
		window.set_event_hook([&hooked, &swallow](Platform::Events::Event &) {
			++hooked;
			return swallow;
		});
		Platform::Events::KeyPressedEvent key(Platform::Events::KeyCode::F5, 0);
		window.on_event(key);
		CHECK(hooked == 1);
		CHECK(window.get_behaviour<CountingWindow>()->intercepted == 0);

		swallow = false;
		window.on_event(key);
		CHECK(hooked == 2);
		CHECK(window.get_behaviour<CountingWindow>()->intercepted == 1);
	}
}
