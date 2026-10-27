#include <doctest.h>

#include "CanvasTestSupport.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/View.h"

using namespace Aquila::UI::Core;
using Aquila::Foundation::FrameScheduler;

namespace {

void settle(Canvas &canvas) {
	canvas.compute();
	canvas.clear_draw_list_dirty();
	FrameScheduler::get()->consume();
}

}

TEST_SUITE("View removal") {
	TEST_CASE("a view may remove children while its canvas is destroyed") {
		struct Owner final : View {
			View *child = nullptr;
			~Owner() override { remove_child(child); }
		};
		CanvasSingletonsScope singletons;
		auto canvas = std::make_unique<Canvas>(200, 200);
		auto *owner = canvas->get_root()->add_child<Owner>();
		owner->child = owner->add_child<View>();
		settle(*canvas);

		canvas.reset();
		CHECK(canvas == nullptr);
	}

	TEST_CASE("a removed view may remove a sibling while it dies") {
		struct RemovesSibling final : View {
			View *owner = nullptr;
			View *sibling = nullptr;
			~RemovesSibling() override { owner->remove_child(sibling); }
		};
		View parent;
		auto *dying = parent.add_child<RemovesSibling>();
		auto *kept = parent.add_child<View>();
		dying->owner = &parent;
		dying->sibling = parent.add_child<View>();

		parent.remove_child(dying);

		REQUIRE(parent.get_children().size() == 1);
		CHECK(parent.get_children().front().get() == kept);
	}

	TEST_CASE("a replaced view may remove a sibling while it dies") {
		struct RemovesSibling final : View {
			View *owner = nullptr;
			View *sibling = nullptr;
			~RemovesSibling() override { owner->remove_child(sibling); }
		};
		View parent;
		View *sibling = parent.add_child<View>();
		auto *dying = parent.add_child<RemovesSibling>();
		dying->owner = &parent;
		dying->sibling = sibling;

		auto incoming = std::make_unique<View>();
		View *incoming_raw = incoming.get();
		View *placed = parent.replace_child(dying, std::move(incoming));

		CHECK(placed == incoming_raw);
		REQUIRE(parent.get_children().size() == 1);
		CHECK(parent.get_children().front().get() == incoming_raw);
		CHECK(incoming_raw->get_parent() == &parent);
	}

	TEST_CASE("remove_child schedules a relayout and a frame") {
		CanvasSingletonsScope singletons;
		Canvas canvas(200, 200);
		View *parent = canvas.get_root()->add_child<View>();
		View *child = parent->add_child<View>();
		settle(canvas);

		parent->remove_child(child);

		CHECK(FrameScheduler::get()->consume());
		canvas.compute();
		CHECK(canvas.is_draw_list_dirty());
	}

	TEST_CASE("detach_child schedules a relayout and a frame") {
		CanvasSingletonsScope singletons;
		Canvas canvas(200, 200);
		View *parent = canvas.get_root()->add_child<View>();
		View *child = parent->add_child<View>();
		settle(canvas);

		auto detached = parent->detach_child(child);
		REQUIRE(detached != nullptr);

		CHECK(FrameScheduler::get()->consume());
		canvas.compute();
		CHECK(canvas.is_draw_list_dirty());
	}
}

TEST_SUITE("View reattach") {
	TEST_CASE("a reattached subtree is marked for redraw as a whole") {
		CanvasSingletonsScope singletons;
		Canvas canvas(200, 200);
		View *parent = canvas.get_root()->add_child<View>();
		View *child = parent->add_child<View>();
		View *grandchild = child->add_child<View>();
		settle(canvas);
		REQUIRE_FALSE(grandchild->is_draw_dirty());

		auto detached = parent->detach_child(child);
		REQUIRE(detached != nullptr);
		parent->add_child(std::move(detached));

		CHECK(child->is_draw_dirty());
		CHECK(grandchild->is_draw_dirty());
	}

	TEST_CASE("replace_child also marks the whole incoming subtree for redraw") {
		CanvasSingletonsScope singletons;
		Canvas canvas(200, 200);
		View *parent = canvas.get_root()->add_child<View>();
		View *old_child = parent->add_child<View>();
		auto incoming = std::make_unique<View>();
		View *incoming_child = incoming->add_child<View>();
		settle(canvas);

		parent->replace_child(old_child, std::move(incoming));

		CHECK(incoming_child->is_draw_dirty());
	}
}
