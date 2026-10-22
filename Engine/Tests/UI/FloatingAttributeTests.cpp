#include <doctest.h>

#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/View.h"

using namespace Aquila::UI;
using namespace Aquila::UI::Core;

TEST_SUITE("Floating XML attributes") {
	TEST_CASE("float attributes build a floating config") {
		LayoutLoader loader;
		auto view = loader.load_string(
			R"(<View float-attach="root" float-element="center-bottom" float-parent="center-bottom" float-offset="0 -16" float-z="30"/>)");
		REQUIRE(view != nullptr);
		REQUIRE(view->has_floating());

		const FloatingConfig &config = view->get_floating();
		CHECK(config.attach_to == FloatingAttachTo::Root);
		CHECK(config.element_point == FloatingAttachPoint::CenterBottom);
		CHECK(config.parent_point == FloatingAttachPoint::CenterBottom);
		CHECK(config.offset.x == doctest::Approx(0.F));
		CHECK(config.offset.y == doctest::Approx(-16.F));
		CHECK(config.z_index == 30);
	}

	TEST_CASE("offset accepts a comma separator") {
		LayoutLoader loader;
		auto view = loader.load_string(R"(<View float-offset="16, 56"/>)");
		REQUIRE(view != nullptr);
		REQUIRE(view->has_floating());
		CHECK(view->get_floating().offset.x == doctest::Approx(16.F));
		CHECK(view->get_floating().offset.y == doctest::Approx(56.F));
	}

	TEST_CASE("a view without float attributes is not floating") {
		LayoutLoader loader;
		auto view = loader.load_string(R"(<View width="100px"/>)");
		REQUIRE(view != nullptr);
		CHECK_FALSE(view->has_floating());
	}
}
