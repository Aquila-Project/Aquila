#include <doctest.h>

#include "Aquila/UI/Core/Canvas.h"
#include "GpuTestSupport.h"
#include "InspectorFixture.h"
#include "LoaderTestSupport.h"

using namespace Aquila;
using namespace Aquila::UI;
using namespace Aquila::UI::Core;
using InspectorFixture::EverythingInspector;

namespace {

struct Inspector {
	LoaderCanvas fixture{ 1200,
						  900,
						  { "/resources/Engine/UI/tokens.aqstyle", "/resources/Engine/UI/widgets.aqstyle",
							"/resources/Engine/UI/editor_chrome.aqstyle" } };
	View *box = nullptr;
	EverythingInspector inspector;

	explicit Inspector(F32 width) {
		test_gpu();
		box = fixture.host().add_child<View>();
		StyleProperties style;
		style.width = StyleLength::pixel(width);
		style.height = StyleLength::pixel(880.F);
		style.flex_direction = FlexDirection::Column;
		style.overflow = Overflow::Hidden;
		style.font_family = "regular";
		box->merge_style(style);
		inspector.build(fixture.loader, *box);
	}

	void layout() {
		fixture.canvas->reload_styles();
		fixture.settle();
	}

	[[nodiscard]] std::vector<std::string> overflowing(bool visible_only) const {
		std::vector<std::string> found;
		const Rect bounds = box->get_absolute_rect();
		collect(*box, bounds, visible_only, found);
		return found;
	}

	static void collect(const View &view, const Rect &bounds, bool visible_only, std::vector<std::string> &found) {
		for (const auto &child : view.get_children()) {
			const ComputedStyle &style = child->get_display_style();
			if (style.display == Display::None) {
				continue;
			}
			const Rect rect = child->get_absolute_rect();
			if (rect.size.x > 0.F && rect.size.y > 0.F &&
				(rect.left() < bounds.left() - 0.5F || rect.right() > bounds.right() + 0.5F)) {
				std::string classes;
				for (const std::string &name : child->get_classes()) {
					classes += "." + name;
				}
				found.push_back(std::string(child->get_type_name()) + classes + " x " + std::to_string(rect.left()) +
								".." + std::to_string(rect.right()));
			}
			const bool sizer = std::ranges::find(child->get_classes(), "dropdown-sizer") != child->get_classes().end();
			const bool clips = style.overflow == Overflow::Clip || style.overflow == Overflow::Hidden;
			if (!sizer && !(visible_only && clips)) {
				collect(*child, bounds, visible_only, found);
			}
		}
	}
};

}

TEST_SUITE("Inspector bounds") {
	TEST_CASE("every inspector row fits a narrow panel") {
		for (const F32 width : { 140.F, 180.F, 220.F, 280.F, 360.F }) {
			CAPTURE(width);
			Inspector inspector(width);
			inspector.layout();
			const std::vector<std::string> overflowing = inspector.overflowing(false);
			for (const std::string &view : overflowing) {
				MESSAGE(width << "px: " << view);
			}
			CHECK(overflowing.empty());
		}
	}

	TEST_CASE("nothing draws outside a panel too narrow to fit the rows") {
		for (const F32 width : { 60.F, 100.F }) {
			CAPTURE(width);
			Inspector inspector(width);
			inspector.layout();
			const std::vector<std::string> overflowing = inspector.overflowing(true);
			for (const std::string &view : overflowing) {
				MESSAGE(width << "px: " << view);
			}
			CHECK(overflowing.empty());
		}
	}
}
