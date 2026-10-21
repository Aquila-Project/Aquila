#include <doctest.h>

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Rendering/ViewMath.h"

using namespace Aquila;
using namespace Aquila::Rendering;

namespace {

RenderView looking_down_z() {
	RenderView view;
	view.view = Mat4(1.F);
	view.projection = Math::perspective_vulkan(Math::radians(60.F), 2.F, 0.1F, 100.F);
	view.projection[1][1] *= -1.F;
	view.position = Vec3(0.F);
	view.forward = Vec3(0.F, 0.F, 1.F);
	view.near_plane = 0.1F;
	view.valid = true;
	return view;
}

const Rect K_VIEWPORT = { .position = { 100.F, 50.F }, .size = { 800.F, 400.F } };

}

TEST_SUITE("ViewMath") {
	TEST_CASE("a point straight ahead lands in the middle of the viewport") {
		const Option<Vec2> screen = project_to_screen(looking_down_z(), K_VIEWPORT, Vec3(0.F, 0.F, 5.F));
		REQUIRE(screen.has_value());
		CHECK(screen->x == doctest::Approx(500.F));
		CHECK(screen->y == doctest::Approx(250.F));
	}

	TEST_CASE("up in the world is up on screen") {
		const Option<Vec2> screen = project_to_screen(looking_down_z(), K_VIEWPORT, Vec3(0.F, 1.F, 5.F));
		REQUIRE(screen.has_value());
		CHECK(screen->y < 250.F);
	}

	TEST_CASE("points behind the camera do not project") {
		CHECK_FALSE(project_to_screen(looking_down_z(), K_VIEWPORT, Vec3(0.F, 0.F, -5.F)).has_value());
	}

	TEST_CASE("a screen ray passes back through the projected point") {
		const RenderView view = looking_down_z();
		const Vec3 world(1.5F, -0.75F, 8.F);
		const Option<Vec2> screen = project_to_screen(view, K_VIEWPORT, world);
		REQUIRE(screen.has_value());

		const auto ray = screen_ray(view, K_VIEWPORT, *screen);
		CHECK(ray.distance_to_point(world) == doctest::Approx(0.F).epsilon(1e-3));
	}

	TEST_CASE("world units per pixel grow with distance") {
		const RenderView view = looking_down_z();
		const F32 near_size = world_units_per_pixel(view, K_VIEWPORT, Vec3(0.F, 0.F, 2.F));
		const F32 far_size = world_units_per_pixel(view, K_VIEWPORT, Vec3(0.F, 0.F, 4.F));
		CHECK(far_size == doctest::Approx(near_size * 2.F));
	}
}
