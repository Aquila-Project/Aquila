#include <doctest.h>

#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/OverlayToolbar.h"
#include "Aquila/UI/Widgets/Separator.h"

#include "CanvasTestSupport.h"

using namespace Aquila::UI;
using namespace Aquila::UI::Core;

namespace {

bool has_class(const View &view, const char *name) {
	const auto &classes = view.get_classes();
	return std::find(classes.begin(), classes.end(), name) != classes.end();
}

struct Stage {
	CanvasSingletonsScope singletons;
	Canvas canvas{ 800, 600 };
	View *area = nullptr;
	OverlayToolbar *toolbar = nullptr;

	Stage() {
		area = canvas.get_root()->add_child<View>();
		StyleProperties area_style;
		area_style.width = StyleLength::pixel(400.F);
		area_style.height = StyleLength::pixel(300.F);
		area->merge_style(area_style);

		toolbar = area->add_child<OverlayToolbar>();
		for (int i = 0; i < 3; ++i) {
			auto *button = toolbar->add_child<Button>();
			StyleProperties button_style;
			button_style.width = StyleLength::pixel(24.F);
			button_style.height = StyleLength::pixel(24.F);
			button->merge_style(button_style);
		}
		settle();
	}

	void settle() {
		for (int i = 0; i < 3; ++i) {
			canvas.update(0.016F);
			canvas.compute();
		}
	}

	void drag_by(Vec2 delta) {
		const Vec2 grab = toolbar->get_absolute_position() + Vec2(4.F, 4.F);
		toolbar->begin_drag(grab);
		toolbar->drag_to(grab + delta);
		settle();
		toolbar->end_drag();
		settle();
	}
};

}

TEST_SUITE("OverlayToolbar") {
	TEST_CASE("the nearest edge is picked from the toolbar centre") {
		const Rect area = { .position = { 0.F, 0.F }, .size = { 400.F, 300.F } };
		CHECK(OverlayToolbar::nearest_edge(area, { .position = { 5.F, 100.F }, .size = { 30.F, 90.F } }) ==
			  OverlayEdge::Left);
		CHECK(OverlayToolbar::nearest_edge(area, { .position = { 360.F, 100.F }, .size = { 30.F, 90.F } }) ==
			  OverlayEdge::Right);
		CHECK(OverlayToolbar::nearest_edge(area, { .position = { 150.F, 4.F }, .size = { 90.F, 30.F } }) ==
			  OverlayEdge::Top);
		CHECK(OverlayToolbar::nearest_edge(area, { .position = { 150.F, 266.F }, .size = { 90.F, 30.F } }) ==
			  OverlayEdge::Bottom);
	}

	TEST_CASE("side edges stack the tools vertically, top and bottom lay them out in a row") {
		OverlayToolbar toolbar;
		auto *separator = toolbar.add_child<Separator>();
		CHECK(has_class(toolbar, "overlay-toolbar-vertical"));
		CHECK(has_class(*separator, "overlay-toolbar-separator-horizontal"));

		toolbar.dock(OverlayEdge::Bottom, 10.F);
		CHECK(toolbar.is_horizontal());
		CHECK(has_class(toolbar, "overlay-toolbar-horizontal"));
		CHECK(has_class(*separator, "overlay-toolbar-separator-vertical"));
		CHECK(toolbar.get_floating().element_point == FloatingAttachPoint::LeftBottom);
	}

	TEST_CASE("the edge can be chosen from a layout") {
		OverlayToolbar toolbar;
		toolbar.apply_xml_attribute("edge", "right");
		toolbar.apply_xml_attribute("along", "40");
		CHECK(toolbar.get_edge() == OverlayEdge::Right);
		CHECK(toolbar.get_along() == doctest::Approx(40.F));
		CHECK(toolbar.get_floating().offset.y == doctest::Approx(40.F));
	}

	TEST_CASE("dropping near another edge docks the toolbar there") {
		Stage stage;
		int docked = 0;
		stage.toolbar->on_docked.connect([&docked](OverlayEdge, float) { ++docked; });

		stage.drag_by({ 340.F, 0.F });
		CHECK(stage.toolbar->get_edge() == OverlayEdge::Right);
		CHECK_FALSE(stage.toolbar->is_dragging());

		stage.drag_by({ -150.F, 240.F });
		CHECK(stage.toolbar->get_edge() == OverlayEdge::Bottom);
		CHECK(stage.toolbar->is_horizontal());
		CHECK(docked == 2);
	}

	TEST_CASE("the dock hint only exists while dragging") {
		Stage stage;
		const size_t children = stage.area->get_children().size();
		const Vec2 grab = stage.toolbar->get_absolute_position() + Vec2(4.F, 4.F);

		stage.toolbar->begin_drag(grab);
		CHECK(stage.area->get_children().size() == children + 1);
		stage.toolbar->end_drag();
		CHECK(stage.area->get_children().size() == children);
	}

	TEST_CASE("a docked toolbar stays inside its area") {
		Stage stage;
		stage.drag_by({ 0.F, 1000.F });
		const Rect area = stage.area->get_absolute_rect();
		const Rect bar = stage.toolbar->get_absolute_rect();
		CHECK(bar.position.y + bar.size.y <= area.position.y + area.size.y + 0.5F);
		CHECK(bar.position.y >= area.position.y - 0.5F);
	}
}
