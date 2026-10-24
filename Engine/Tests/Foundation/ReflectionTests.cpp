#include <doctest.h>

#include "Aquila/Foundation/Reflection/TypeInfo.h"

using namespace Aquila::Reflection;

namespace {

enum class Mode { Fast, Slow };

struct Inner {
	float gain = 1.F;
};

struct Sample {
	float amount = 2.F;
	int count = 3;
	bool enabled = true;
	Vec3 direction{ 0.F, 1.F, 0.F };
	std::string label = "sample";
	Mode mode = Mode::Fast;
	Inner inner;

	float get_clamped() const { return clamped; }
	void set_clamped(float value) { clamped = value < 0.F ? 0.F : value; }

	float clamped = 1.F;
};

TypeInfo make_info() {
	TypeInfo info("Sample");
	TypeBuilder<Sample>(info)
		.property("Amount", &Sample::amount, { .min = 0.F, .max = 10.F, .speed = 0.5F, .precision = 1 })
		.property("Count", &Sample::count)
		.property("Enabled", &Sample::enabled, { .toggle = true })
		.property("Direction", &Sample::direction)
		.property("Label", &Sample::label)
		.property("Mode", &Sample::mode)
		.options({ { "Fast", 0 }, { "Slow", 1 } })
		.property("Gain", [](Sample &s) -> float & { return s.inner.gain; })
		.property("Clamped", &Sample::get_clamped, &Sample::set_clamped)
		.property("Detail", &Sample::amount)
		.visible_if([](const Sample &s) { return s.enabled; });
	return info;
}

}

TEST_SUITE("Reflection") {
	TEST_CASE("kinds are deduced from the C++ type") {
		const TypeInfo info = make_info();
		CHECK(info.find("Amount")->kind == PropertyKind::Float);
		CHECK(info.find("Count")->kind == PropertyKind::Int);
		CHECK(info.find("Enabled")->kind == PropertyKind::Bool);
		CHECK(info.find("Direction")->kind == PropertyKind::Vec3);
		CHECK(info.find("Label")->kind == PropertyKind::String);
		CHECK(info.find("Mode")->kind == PropertyKind::Enum);
		CHECK(info.find("Nope") == nullptr);
	}

	TEST_CASE("properties keep registration order and hints") {
		const TypeInfo info = make_info();
		CHECK(info.get_name() == "Sample");
		CHECK(info.get_properties().front().name == "Amount");
		CHECK(info.get_properties().back().name == "Detail");

		const Property &amount = *info.find("Amount");
		CHECK(amount.hints.min == doctest::Approx(0.F));
		CHECK(amount.hints.max == doctest::Approx(10.F));
		CHECK(amount.hints.precision == 1);
		CHECK(info.find("Enabled")->hints.toggle);
	}

	TEST_CASE("get reads through member pointers") {
		const TypeInfo info = make_info();
		Sample sample;
		CHECK(std::get<F32>(info.find("Amount")->get(&sample)) == doctest::Approx(2.F));
		CHECK(std::get<Int32>(info.find("Count")->get(&sample)) == 3);
		CHECK(std::get<bool>(info.find("Enabled")->get(&sample)));
		CHECK(std::get<Vec3>(info.find("Direction")->get(&sample)).y == doctest::Approx(1.F));
		CHECK(std::get<std::string>(info.find("Label")->get(&sample)) == "sample");
	}

	TEST_CASE("set writes through member pointers") {
		const TypeInfo info = make_info();
		Sample sample;
		info.find("Amount")->set(&sample, PropertyValue{ 7.5F });
		info.find("Count")->set(&sample, PropertyValue{ Int32{ 9 } });
		info.find("Enabled")->set(&sample, PropertyValue{ false });
		info.find("Label")->set(&sample, PropertyValue{ std::string("changed") });
		CHECK(sample.amount == doctest::Approx(7.5F));
		CHECK(sample.count == 9);
		CHECK_FALSE(sample.enabled);
		CHECK(sample.label == "changed");
	}

	TEST_CASE("enums travel as integers and keep their option names") {
		const TypeInfo info = make_info();
		Sample sample;
		const Property &mode = *info.find("Mode");
		REQUIRE(mode.options.size() == 2);
		CHECK(mode.options[1].name == "Slow");

		mode.set(&sample, PropertyValue{ Int32{ 1 } });
		CHECK(sample.mode == Mode::Slow);
		CHECK(std::get<Int32>(mode.get(&sample)) == 1);
	}

	TEST_CASE("projections reach nested members") {
		const TypeInfo info = make_info();
		Sample sample;
		info.find("Gain")->set(&sample, PropertyValue{ 4.F });
		CHECK(sample.inner.gain == doctest::Approx(4.F));
		CHECK(std::get<F32>(info.find("Gain")->get(&sample)) == doctest::Approx(4.F));
	}

	TEST_CASE("getter/setter pairs run the setter's logic") {
		const TypeInfo info = make_info();
		Sample sample;
		info.find("Clamped")->set(&sample, PropertyValue{ -5.F });
		CHECK(sample.clamped == doctest::Approx(0.F));
		CHECK(std::get<F32>(info.find("Clamped")->get(&sample)) == doctest::Approx(0.F));
	}

	TEST_CASE("visible_if is evaluated against the live instance") {
		const TypeInfo info = make_info();
		Sample sample;
		const Property &detail_property = *info.find("Detail");
		CHECK(detail_property.is_visible(&sample));
		sample.enabled = false;
		CHECK_FALSE(detail_property.is_visible(&sample));
		CHECK(info.find("Amount")->is_visible(&sample));
	}

	TEST_CASE("read-only properties expose a value and ignore writes") {
		TypeInfo info("Sample");
		TypeBuilder<Sample>(info).read_only("Summary", [](const Sample &s) { return s.label + "!"; });
		const Property &summary = *info.find("Summary");
		Sample sample;

		CHECK(summary.kind == PropertyKind::String);
		CHECK(summary.hints.read_only);
		CHECK(std::get<std::string>(summary.get(&sample)) == "sample!");
		summary.set(&sample, PropertyValue{ std::string("changed") });
		CHECK(sample.label == "sample");
	}

	TEST_CASE("choices store a value and show its label") {
		TypeInfo info("Sample");
		TypeBuilder<Sample>(info).choice("Label", &Sample::label, { "", "sample", "other" },
										 { "None", "Sample", "Other" });
		const Property &label = *info.find("Label");
		Sample sample;

		CHECK(label.kind == PropertyKind::Enum);
		REQUIRE(label.options.size() == 3);
		CHECK(label.options[2].name == "Other");
		CHECK(std::get<Int32>(label.get(&sample)) == 1);
		label.set(&sample, PropertyValue{ Int32{ 2 } });
		CHECK(sample.label == "other");
		label.set(&sample, PropertyValue{ Int32{ 7 } });
		CHECK(sample.label == "other");
		sample.label = "unlisted";
		CHECK(std::get<Int32>(label.get(&sample)) == 0);
	}
}
