#include <doctest.h>

#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Rendering/DrawList.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Widgets/SelectableTextView.h"
#include "CanvasTestSupport.h"
#include "GpuTestSupport.h"

using namespace Aquila::UI::Core;

TEST_SUITE("SelectableTextView") {
	TEST_CASE("select all then read back the whole text") {
		SelectableTextView view;
		view.add_line("alpha");
		view.add_line("beta");
		view.add_line("gamma");

		view.select_all();
		CHECK(view.get_selected_text() == "alpha\nbeta\ngamma");
	}

	TEST_CASE("nothing is selected until the user selects") {
		SelectableTextView view;
		view.add_line("alpha");
		CHECK(view.get_selected_text().empty());
	}

	TEST_CASE("clearing the selection empties it") {
		SelectableTextView view;
		view.add_line("alpha");
		view.select_all();
		view.clear_selection();
		CHECK(view.get_selected_text().empty());
	}

	TEST_CASE("clear removes lines and selection") {
		SelectableTextView view;
		view.add_line("alpha");
		view.select_all();
		view.clear();

		CHECK(view.get_line_count() == 0);
		CHECK(view.get_selected_text().empty());
	}

	TEST_CASE("removing lines from the front keeps the selection on the same text") {
		SelectableTextView view;
		view.add_line("alpha");
		view.add_line("beta");
		view.add_line("gamma");
		view.select_all();

		view.remove_front(1);
		CHECK(view.get_line_count() == 2);
		CHECK(view.get_selected_text() == "beta\ngamma");
	}

	TEST_CASE("removing more lines than exist is harmless") {
		SelectableTextView view;
		view.add_line("alpha");
		view.remove_front(10);
		CHECK(view.get_line_count() == 0);
	}

	TEST_CASE("lines and tags draw in their style class colors") {
		test_gpu();
		CanvasSingletonsScope singletons;
		Canvas canvas(400, 200);
		REQUIRE(Aquila::UI::StyleParser::LoadString(
			".line-warn { color: rgb(1, 0, 0); } .tag-warn { color: rgb(0, 1, 0); }", canvas.get_style_sheet()));
		auto *view = canvas.get_root()->add_child<SelectableTextView>();
		Aquila::UI::StyleProperties size;
		size.font_family = "regular";
		size.width = Aquila::UI::StyleLength::pixel(380.F);
		size.height = Aquila::UI::StyleLength::pixel(180.F);
		view->set_style(size);
		view->add_line("[WARN] hi", "line-warn", 0, 6, "tag-warn");
		view->add_line("plain");
		view->add_line("unknown", "nope");
		canvas.compute();

		Aquila::UI::Rendering::DrawList list;
		view->on_draw_self(list);
		std::map<std::string, Vec4> drawn;
		for (const auto &command : list.get_commands()) {
			if (const auto *text = std::get_if<Aquila::UI::Rendering::TextCmd>(&command)) {
				drawn[text->text] = text->color;
			}
		}
		const Vec4 own = view->get_display_style().color;
		REQUIRE(drawn.contains("[WARN]"));
		CHECK(drawn["[WARN]"] == Vec4(0.F, 1.F, 0.F, 1.F));
		REQUIRE(drawn.contains(" hi"));
		CHECK(drawn[" hi"] == Vec4(1.F, 0.F, 0.F, 1.F));
		REQUIRE(drawn.contains("plain"));
		CHECK(drawn["plain"] == own);
		REQUIRE(drawn.contains("unknown"));
		CHECK(drawn["unknown"] == own);
	}
}
