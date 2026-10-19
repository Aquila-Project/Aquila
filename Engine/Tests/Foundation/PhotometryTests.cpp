#include <doctest.h>

#include "Aquila/Foundation/Photometry.h"

using namespace Aquila;
namespace Photometry = Aquila::Foundation::Photometry;

TEST_SUITE("Photometry") {
	TEST_CASE("sunny 16 is about EV 15") {
		CHECK(Photometry::ev100(16.F, 1.F / 125.F, 100.F) == doctest::Approx(14.966F).epsilon(0.001));
		CHECK(Photometry::ev100(16.F, 1.F / 125.F, 200.F) == doctest::Approx(13.966F).epsilon(0.001));
	}

	TEST_CASE("a white diffuse plane under the noon sun lands near 0.8 at EV 15") {
		const F32 luminance = 100000.F / Math::PI;
		CHECK(luminance * Photometry::exposure(Photometry::DAYLIGHT_EV100) == doctest::Approx(0.8095F).epsilon(0.001));
		CHECK(Photometry::exposure(14.F) == doctest::Approx(2.F * Photometry::exposure(15.F)));
	}

	TEST_CASE("lumens spread over the sphere or the cone") {
		CHECK(Photometry::point_intensity(4.F * Math::PI) == doctest::Approx(1.F));
		CHECK(Photometry::spot_intensity(800.F, Math::HALF_PI) == doctest::Approx(800.F / Math::TAU));
		CHECK(Photometry::spot_intensity(800.F, Math::radians(20.F)) > Photometry::spot_intensity(800.F, 0.7F));
	}

	TEST_CASE("a glowing sphere lights like a sphere light as bright as its surface") {
		const F32 radius = 0.5F;
		const F32 area = 4.F * Math::PI * radius * radius;
		CHECK(Photometry::emitter_intensity(1000.F, area) == doctest::Approx(1000.F * Math::PI * radius * radius));
		CHECK(Photometry::emitter_radius(area) == doctest::Approx(radius));
		CHECK(4.F * Math::PI * Photometry::emitter_intensity(1000.F, area) == doctest::Approx(Math::PI * 1000.F * area));
	}
}
