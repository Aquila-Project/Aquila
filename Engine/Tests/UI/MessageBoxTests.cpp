#include <doctest.h>

#include "LoaderTestSupport.h"

#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/MessageBox.h"

using namespace Aquila::UI::Core;

namespace {

bool has_class(const View &view, std::string_view name) {
	return std::ranges::find(view.get_classes(), name) != view.get_classes().end();
}

const Label *find_label(const View &view, std::string_view cls) {
	if (const auto *label = dynamic_cast<const Label *>(&view); label != nullptr && has_class(view, cls)) {
		return label;
	}
	for (const auto &child : view.get_children()) {
		if (const Label *found = find_label(*child, cls)) {
			return found;
		}
	}
	return nullptr;
}

}

TEST_SUITE("MessageBox") {
	TEST_CASE("each kind builds its box with the text") {
		LoaderCanvas fixture;
		View *error = create_message_box(fixture.host(), MessageKind::Error, "Missing reference");
		View *warning = create_message_box(fixture.host(), MessageKind::Warning, "Careful");
		View *info = create_message_box(fixture.host(), MessageKind::Info, "Hello");
		REQUIRE(error != nullptr);
		REQUIRE(warning != nullptr);
		REQUIRE(info != nullptr);

		CHECK(has_class(*error, "message-box"));
		CHECK(has_class(*error, "message-box-error"));
		CHECK(has_class(*warning, "message-box-warning"));
		CHECK(has_class(*info, "message-box-info"));
		const Label *text = find_label(*error, "message-box-text");
		REQUIRE(text != nullptr);
		CHECK(text->get_text() == "Missing reference");
	}

	TEST_CASE("outside a canvas nothing is built") {
		View loose;
		CHECK(create_message_box(loose, MessageKind::Error, "nope") == nullptr);
	}
}
