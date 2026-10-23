#include <doctest.h>

#include "Aquila/Foundation/Registration.h"

using Aquila::Foundation::Registration;
using Aquila::Foundation::RegistrationOwner;

TEST_SUITE("Registration") {
	TEST_CASE("destroying the handle unregisters once") {
		RegistrationOwner owner;
		int count = 0;
		{
			Registration registration = owner.make([&count] { ++count; });
			CHECK(registration.is_active());
			CHECK(count == 0);
		}
		CHECK(count == 1);
	}

	TEST_CASE("reset unregisters right away and only once") {
		RegistrationOwner owner;
		int count = 0;
		Registration registration = owner.make([&count] { ++count; });
		registration.reset();
		CHECK(count == 1);
		CHECK_FALSE(registration.is_active());
		registration.reset();
		CHECK(count == 1);
	}

	TEST_CASE("a default handle is inactive and resets to nothing") {
		Registration registration;
		CHECK_FALSE(registration.is_active());
		registration.reset();
	}

	TEST_CASE("moving hands the registration over") {
		RegistrationOwner owner;
		int count = 0;
		Registration first = owner.make([&count] { ++count; });
		Registration second = std::move(first);
		CHECK_FALSE(first.is_active());
		CHECK(second.is_active());
		first.reset();
		CHECK(count == 0);
		second.reset();
		CHECK(count == 1);
	}

	TEST_CASE("assigning over a handle unregisters what it held") {
		RegistrationOwner owner;
		int old_count = 0;
		int new_count = 0;
		Registration registration = owner.make([&old_count] { ++old_count; });
		registration = owner.make([&new_count] { ++new_count; });
		CHECK(old_count == 1);
		CHECK(new_count == 0);
		CHECK(registration.is_active());
	}

	TEST_CASE("a handle that outlives its registry does nothing") {
		int count = 0;
		Registration registration;
		{
			RegistrationOwner owner;
			registration = owner.make([&count] { ++count; });
		}
		CHECK_FALSE(registration.is_active());
		registration.reset();
		CHECK(count == 0);
	}

	TEST_CASE("resetting from inside the unregister callback doesn't run it again") {
		RegistrationOwner owner;
		int count = 0;
		Registration registration;
		registration = owner.make([&count, &registration] {
			++count;
			registration.reset();
		});
		registration.reset();
		CHECK(count == 1);
	}

	TEST_CASE("clearing a list of handles unregisters all of them") {
		RegistrationOwner owner;
		std::vector<int> removed;
		std::vector<Registration> registrations;
		for (int i = 0; i < 3; ++i) {
			registrations.push_back(owner.make([&removed, i] { removed.push_back(i); }));
		}
		registrations.clear();
		CHECK(removed.size() == 3);
	}
}
