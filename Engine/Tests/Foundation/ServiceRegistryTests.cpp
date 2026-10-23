#include <doctest.h>

#include "Aquila/Foundation/ServiceRegistry.h"

using Aquila::Foundation::Registration;
using Aquila::Foundation::ServiceRegistry;

namespace {

struct Physics {
	int steps = 0;
};

struct Audio {};

class IClock {
  public:
	virtual ~IClock() = default;
	[[nodiscard]] virtual int now() const = 0;
};

class FixedClock final : public IClock {
  public:
	[[nodiscard]] int now() const override { return 42; }
};

}

TEST_SUITE("ServiceRegistry") {
	TEST_CASE("nothing is found before it's provided") {
		const ServiceRegistry services;
		CHECK(services.find<Physics>() == nullptr);
	}

	TEST_CASE("a service is found until its registration goes away") {
		ServiceRegistry services;
		Physics physics;
		{
			const Registration registration = services.provide<Physics>(physics);
			CHECK(registration.is_active());
			CHECK(services.find<Physics>() == &physics);
			CHECK(services.find<Audio>() == nullptr);
		}
		CHECK(services.find<Physics>() == nullptr);
	}

	TEST_CASE("an implementation is found as the type it was provided as") {
		ServiceRegistry services;
		FixedClock clock;
		const Registration registration = services.provide<IClock>(clock);
		REQUIRE(services.find<IClock>() != nullptr);
		CHECK(services.find<IClock>()->now() == 42);
		CHECK(services.find<FixedClock>() == nullptr);
	}

	TEST_CASE("a second provider is refused and the first one stays") {
		ServiceRegistry services;
		Physics first;
		Physics second;
		const Registration kept = services.provide<Physics>(first);
		Registration refused = services.provide<Physics>(second);
		CHECK(kept.is_active());
		CHECK_FALSE(refused.is_active());
		CHECK(services.find<Physics>() == &first);
		refused.reset();
		CHECK(services.find<Physics>() == &first);
	}

	TEST_CASE("a type can be provided again once its provider left") {
		ServiceRegistry services;
		Physics first;
		Physics second;
		Registration registration = services.provide<Physics>(first);
		registration.reset();
		registration = services.provide<Physics>(second);
		CHECK(registration.is_active());
		CHECK(services.find<Physics>() == &second);
	}

	TEST_CASE("a registration that outlives the registry is harmless") {
		Physics physics;
		Registration registration;
		{
			ServiceRegistry services;
			registration = services.provide<Physics>(physics);
		}
		CHECK_FALSE(registration.is_active());
		registration.reset();
	}
}
