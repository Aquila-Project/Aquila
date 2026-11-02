#include <doctest.h>

#include "Aquila/UI/Core/DockLayoutBuilder.h"
#include "Aquila/UI/Widgets/DockNode.h"
#include "Aquila/UI/Widgets/DockPanel.h"
#include "Aquila/UI/Widgets/DockSpace.h"
#include "LoaderTestSupport.h"

using namespace Aquila::UI;
using namespace Aquila::UI::Core;
using namespace Aquila::UI::Core::DockLayout;

namespace {

constexpr F32 K_TILE = 104.F;

void frame(const LoaderCanvas &fixture) {
	fixture.canvas->update(0.016F);
	fixture.canvas->compute();
}

View *add_wrapping_row(View &parent, int tiles) {
	auto *row = parent.add_child<View>();
	StyleProperties style;
	style.flex_direction = FlexDirection::Row;
	style.flex_wrap = FlexWrap::Wrap;
	style.width = StyleLength::percent(100.F);
	style.gap = 4.F;
	row->merge_style(style);
	for (int i = 0; i < tiles; ++i) {
		auto *tile = row->add_child<View>();
		StyleProperties tile_style;
		tile_style.width = StyleLength::pixel(K_TILE);
		tile_style.height = StyleLength::pixel(40.F);
		tile->merge_style(tile_style);
	}
	return row;
}

DockPanel *find_panel(View &view, std::string_view title) {
	for (const auto &child : view.get_children()) {
		if (auto *panel = dynamic_cast<DockPanel *>(child.get()); panel != nullptr && panel->get_title() == title) {
			return panel;
		}
		if (DockPanel *found = find_panel(*child, title)) {
			return found;
		}
	}
	return nullptr;
}

}

TEST_SUITE("Flex wrap") {
	TEST_CASE("a wrapping row moved to a narrower dock area wraps to it in the same frame") {
		LoaderCanvas fixture(1200, 700);
		auto *space = fixture.host().add_child<DockSpace>();
		StyleProperties fill;
		fill.width = StyleLength::grow();
		fill.height = StyleLength::grow();
		space->merge_style(fill);
		space->set_panel_factory([](const std::string &id) { return std::make_unique<DockPanel>(id); });
		space->apply_layout(layout(row({ { 0.2F, tabs({ "left" }) },
										 { 0.6F, column({ { 0.7F, tabs({ "middle" }) }, { 0.3F, tabs({ "wide" }) } }) },
										 { 0.2F, tabs({ "narrow" }) } })));
		for (int i = 0; i < 3; ++i) {
			frame(fixture);
		}
		DockPanel *wide = find_panel(fixture.host(), "wide");
		DockPanel *narrow = find_panel(fixture.host(), "narrow");
		REQUIRE(wide != nullptr);
		REQUIRE(narrow != nullptr);
		View *row = add_wrapping_row(*wide, 24);
		for (int i = 0; i < 3; ++i) {
			frame(fixture);
		}
		REQUIRE(row->get_layout_rect().size.x > 400.F);

		DockNode *target = space->find_node_of(narrow);
		REQUIRE(target != nullptr);
		const F32 narrow_width = target->get_absolute_rect().size.x;
		space->move_panel(wide, target);
		target->set_active_panel_by_ptr(wide);
		target->close_panel(narrow);
		frame(fixture);

		const Rect area = target->get_absolute_rect();
		CHECK(area.size.x == doctest::Approx(narrow_width).epsilon(0.01));
		CHECK(row->get_layout_rect().size.x <= area.size.x + 0.5F);
		for (const auto &tile : row->get_children()) {
			const Rect rect = tile->get_absolute_rect();
			CHECK(rect.position.x + rect.size.x <= area.position.x + area.size.x + 0.5F);
		}
	}

	TEST_CASE("a wrapping row that sizes to its content is never narrower than its widest item") {
		LoaderCanvas fixture(400, 300);
		auto *box = fixture.host().add_child<View>();
		StyleProperties box_style;
		box_style.width = StyleLength::pixel(60.F);
		box->merge_style(box_style);
		auto *row = add_wrapping_row(*box, 3);
		StyleProperties fit;
		fit.width = StyleLength::Auto();
		row->merge_style(fit);
		for (int i = 0; i < 3; ++i) {
			frame(fixture);
		}

		CHECK(row->get_layout_rect().size.x >= K_TILE - 0.5F);
		for (const auto &tile : row->get_children()) {
			CHECK(tile->get_layout_rect().position.x == doctest::Approx(0.F));
		}
	}
}
