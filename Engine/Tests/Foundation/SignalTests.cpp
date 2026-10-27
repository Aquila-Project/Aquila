#include <doctest.h>

#include "Aquila/Foundation/Signal.h"

TEST_SUITE("Signal") {
	TEST_CASE("a lifetime expires with its signal") {
		WeakRef<void> lifetime;
		{
			Signal<void()> signal;
			lifetime = signal.lifetime();
			CHECK_FALSE(lifetime.expired());
		}
		CHECK(lifetime.expired());
	}

	TEST_CASE("a copy doesn't keep the original's lifetime alive") {
		WeakRef<void> lifetime;
		Option<Signal<void()>> copy;
		{
			Signal<void()> original;
			lifetime = original.lifetime();
			copy.emplace(original);
		}
		CHECK(lifetime.expired());
		CHECK_FALSE(copy->lifetime().expired());
	}

	TEST_CASE("asking twice gives the same lifetime") {
		Signal<void(int)> signal;
		const WeakRef<void> first = signal.lifetime();
		const WeakRef<void> second = signal.lifetime();
		CHECK_FALSE(first.owner_before(second));
		CHECK_FALSE(second.owner_before(first));
	}
}
