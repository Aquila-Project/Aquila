#include <doctest.h>

#include "LoaderTestSupport.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/UI/Widgets/AssetSlot.h"
#include "Aquila/UI/Widgets/Label.h"

using namespace Aquila::UI::Core;
using Aquila::Foundation::FrameScheduler;

namespace {

bool has_class(const View &view, std::string_view name) {
	const auto &classes = view.get_classes();
	return std::ranges::find(classes, name) != classes.end();
}

const Label &label_of(const AssetSlot &slot) {
	for (const auto &child : slot.get_children()) {
		if (const auto *label = dynamic_cast<const Label *>(child.get())) {
			return *label;
		}
	}
	FAIL("the slot has no label");
	throw std::logic_error("unreachable");
}

AssetPayload texture(const std::string &path) {
	return AssetPayload{ path, "texture", path.substr(path.find_last_of('/') + 1) };
}

}

TEST_SUITE("AssetSlot missing values") {
	TEST_CASE("a slot whose file is gone says so and keeps the name") {
		LoaderCanvas fixture;
		MarkupFiles files;
		auto &slot = *fixture.host().add_child<AssetSlot>("texture");

		slot.set_value(texture("/markup-tests/gone.png"));
		CHECK(slot.is_missing());
		CHECK(has_class(slot, "asset-slot-missing"));
		CHECK(slot.get_tooltip() == "Missing: /markup-tests/gone.png");
		CHECK(label_of(slot).get_text() == "gone.png");
	}

	TEST_CASE("a slot whose file is there looks filled") {
		LoaderCanvas fixture;
		MarkupFiles files;
		files.write("/markup-tests/wood.png", "x");
		auto &slot = *fixture.host().add_child<AssetSlot>("texture");

		slot.set_value(texture("/markup-tests/wood.png"));
		CHECK_FALSE(slot.is_missing());
		CHECK_FALSE(has_class(slot, "asset-slot-missing"));
		CHECK(has_class(slot, "asset-slot-filled"));
		CHECK(slot.get_tooltip() == "/markup-tests/wood.png");
	}

	TEST_CASE("checking again notices files that go away and come back") {
		LoaderCanvas fixture;
		MarkupFiles files;
		files.write("/markup-tests/wood.png", "x");
		auto &slot = *fixture.host().add_child<AssetSlot>("texture");
		slot.set_value(texture("/markup-tests/wood.png"));

		std::filesystem::remove(files.m_dir / "wood.png");
		CHECK_FALSE(slot.is_missing());
		AssetSlot::check_missing();
		CHECK(slot.is_missing());
		CHECK(has_class(slot, "asset-slot-missing"));

		files.write("/markup-tests/wood.png", "x");
		AssetSlot::check_missing();
		CHECK_FALSE(slot.is_missing());
		CHECK_FALSE(has_class(slot, "asset-slot-missing"));
	}

	TEST_CASE("checking asks for a frame only when a slot changed") {
		LoaderCanvas fixture;
		MarkupFiles files;
		files.write("/markup-tests/wood.png", "x");
		auto &slot = *fixture.host().add_child<AssetSlot>("texture");
		slot.set_value(texture("/markup-tests/wood.png"));
		fixture.settle();
		FrameScheduler::get()->consume();

		AssetSlot::check_missing();
		CHECK_FALSE(FrameScheduler::get()->is_pending());

		std::filesystem::remove(files.m_dir / "wood.png");
		AssetSlot::check_missing();
		CHECK(FrameScheduler::get()->is_pending());
	}

	TEST_CASE("an adapter decides what exists") {
		bool alive = true;
		AssetSlot::set_adapter("thing", { .exists = [&alive](const std::string &) { return alive; } });
		LoaderCanvas fixture;
		auto &slot = *fixture.host().add_child<AssetSlot>("thing");
		slot.set_value(AssetPayload{ "/nowhere/7", "thing", "Thing 7" });
		CHECK_FALSE(slot.is_missing());

		alive = false;
		AssetSlot::check_missing();
		CHECK(slot.is_missing());

		AssetSlot::set_adapter("thing", {});
		CHECK(AssetSlot::find_adapter("thing") == nullptr);
	}

	TEST_CASE("the missing state reports only when it flips") {
		LoaderCanvas fixture;
		MarkupFiles files;
		files.write("/markup-tests/wood.png", "x");
		auto &slot = *fixture.host().add_child<AssetSlot>("texture");
		std::vector<bool> reported;
		slot.on_missing_changed.connect([&reported](bool missing) { reported.push_back(missing); });

		slot.set_value(texture("/markup-tests/wood.png"));
		AssetSlot::check_missing();
		CHECK(reported.empty());

		slot.set_value(texture("/markup-tests/gone.png"));
		AssetSlot::check_missing();
		slot.clear();
		CHECK(reported == std::vector<bool>{ true, false });
	}

	TEST_CASE("values that aren't VFS paths and empty slots are never missing") {
		LoaderCanvas fixture;
		auto &slot = *fixture.host().add_child<AssetSlot>("shader");
		slot.set_value(AssetPayload{ "Basic.slang", "shader", "Basic.slang" });
		CHECK_FALSE(slot.is_missing());

		slot.set_value(AssetPayload{ "/markup-tests/nothing.slang", "shader", "nothing.slang" });
		CHECK(slot.is_missing());
		slot.clear();
		CHECK_FALSE(slot.is_missing());
		CHECK_FALSE(has_class(slot, "asset-slot-missing"));
		CHECK(slot.get_tooltip().empty());
	}
}
