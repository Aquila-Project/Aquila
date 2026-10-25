#include <doctest.h>

#include "Aquila/Foundation/Log.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/Label.h"

using namespace Aquila::UI::Core;
using Aquila::Foundation::Logger;
using Aquila::Foundation::LogLevel;

namespace {

struct Loaded {
	Unique<View> root;
	std::string log;
};

Loaded load(std::string_view xml) {
	std::ostringstream stream;
	const LogLevel previous = Logger::get_log_level();
	Logger::set_sink(&stream);
	Logger::set_log_level(LogLevel::Warning);
	LayoutLoader loader;
	Unique<View> root = loader.load_string(xml);
	Logger::set_sink(nullptr);
	Logger::set_log_level(previous);
	return { std::move(root), stream.str() };
}

std::vector<std::string> child_ids(const View &view) {
	std::vector<std::string> ids;
	for (const auto &child : view.get_children()) {
		ids.push_back(child->get_id());
	}
	return ids;
}

}

TEST_SUITE("Components") {
	TEST_CASE("a component defined in the file is used as a tag") {
		const Loaded loaded = load(R"(
<Component name="Card"><View class="card"><Label id="title">Hi</Label></View></Component>
<View id="root"><Card id="first"/><Card id="second" class="wide"/></View>)");
		REQUIRE(loaded.root != nullptr);
		CHECK_MESSAGE(loaded.log.empty(), loaded.log);
		REQUIRE(child_ids(*loaded.root) == std::vector<std::string>{ "first", "second" });
		CHECK(loaded.root->get_children()[0]->get_classes() == std::vector<std::string>{ "card" });
		CHECK(loaded.root->get_children()[1]->get_classes() == std::vector<std::string>{ "card", "wide" });
		const auto *label = dynamic_cast<const Label *>(loaded.root->get_children()[1]->get_children()[0].get());
		REQUIRE(label != nullptr);
		CHECK(label->get_text() == "Hi");
	}

	TEST_CASE("instance children go into the slot") {
		const Loaded loaded = load(R"(
<Component name="Box"><View class="box"><Label>Head</Label><Slot/><Label>Foot</Label></View></Component>
<Box id="b"><View id="inner"/><View id="second"/></Box>)");
		REQUIRE(loaded.root != nullptr);
		CHECK_MESSAGE(loaded.log.empty(), loaded.log);
		CHECK(loaded.root->get_id() == "b");
		CHECK(child_ids(*loaded.root) == std::vector<std::string>{ "", "inner", "second", "" });
	}

	TEST_CASE("a component can be built from another component") {
		const Loaded loaded = load(R"(
<Component name="A"><View class="a"/></Component>
<Component name="B"><A class="b"/></Component>
<B id="x" class="c"/>)");
		REQUIRE(loaded.root != nullptr);
		CHECK_MESSAGE(loaded.log.empty(), loaded.log);
		CHECK(loaded.root->get_id() == "x");
		CHECK(loaded.root->get_classes() == std::vector<std::string>{ "a", "b", "c" });
	}

	TEST_CASE("a component that uses itself is reported and builds nothing") {
		const Loaded loaded = load(R"(
<Component name="Loop"><View><Loop/></View></Component>
<Component name="Ping"><Pong/></Component>
<Component name="Pong"><View><Ping/></View></Component>
<View id="root"><Loop/><Ping/></View>)");
		REQUIRE(loaded.root != nullptr);
		CHECK(loaded.log.find("'Loop' uses itself") != std::string::npos);
		CHECK(loaded.log.find("'Ping' uses itself") != std::string::npos);
	}

	TEST_CASE("broken definitions are reported") {
		CHECK(load(R"(<Component><View/></Component><View/>)").log.find("needs a 'name'") != std::string::npos);
		CHECK(load(R"(<Component name="Two"><View/><View/></Component><View/>)").log.find("exactly one root") !=
			  std::string::npos);
		CHECK(load(R"(<Component name="S"><View><Slot/><Slot/></View></Component><View/>)").log.find("one <Slot/>") !=
			  std::string::npos);
		CHECK(load(R"(<Component name="D"><View/></Component><Component name="D"><View/></Component><View/>)")
				  .log.find("a second <Component> named 'D'") != std::string::npos);
		CHECK(load(R"(<Component name="Button"><View/></Component><View/>)").log.find("the name of a widget") !=
			  std::string::npos);
		CHECK(load(R"(<View><Slot/></View>)").log.find("only works inside a <Component>") != std::string::npos);
		CHECK(load(R"(<Component name="N"><View/></Component><N><View/></N>)").log.find("has no <Slot/>") !=
			  std::string::npos);
		CHECK(load(R"(<Component name="Q"><View/></Component><Q title="x"/>)")
				  .log.find("doesn't take the attribute 'title'") != std::string::npos);
	}
}
